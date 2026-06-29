#include "infra/persistence/ConnectionPool.h"

#include "utils/Loggable.h"

ConnectionPool::ConnectionPool(const std::string& conninfo, std::size_t pool_size,
                               std::chrono::milliseconds wait_timeout)
    : conninfo_(conninfo), wait_timeout_(wait_timeout) {
    if (pool_size == 0) throw std::invalid_argument("pool_size must be greater than zero");
    slots_.resize(pool_size);  // slots start empty; connections are opened lazily in acquire()

    // Best-effort pre-warm. If the database is reachable we open the connections now so the first
    // requests are fast; if it is NOT reachable we DO NOT abort — the server stays up and each
    // slot connects on demand once the database returns. This is what lets the backend survive a
    // database that is down or restarting at startup.
    std::size_t opened = 0;
    for (auto& slot : slots_) {
        try {
            slot.conn = open_connection();
            ++opened;
        } catch (const std::exception& e) {
            log(utils::LogLevel::WARN,
                "Database not reachable at startup; connections will be established lazily. "
                "Reason: ",
                e.what());
            break;  // stop hammering a down database; remaining slots connect on demand
        }
    }
    log(utils::LogLevel::INFO, "Connection pool ready (", opened, "/", pool_size,
        " connections pre-warmed, rest lazy)");
}

std::unique_ptr<pqxx::connection> ConnectionPool::open_connection() const {
    auto connection = std::make_unique<pqxx::connection>(conninfo_);
    if (!connection->is_open()) throw std::runtime_error("Failed to open database connection");
    return connection;
}

pqxx::connection& ConnectionPool::acquire() {
    Slot* chosen = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto has_free_slot = [&]() {
            for (const auto& slot : slots_) {
                if (!slot.in_use) return true;
            }
            return false;
        };
        auto find_slot = [&]() -> Slot* {
            for (auto& slot : slots_) {
                if (!slot.in_use) return &slot;
            }
            return nullptr;
        };

        chosen = find_slot();
        while (!chosen) {
            if (!cv_.wait_for(lock, wait_timeout_, has_free_slot)) {
                throw std::runtime_error(
                    "Connection pool timeout waiting for available connection");
            }
            chosen = find_slot();
        }
        chosen->in_use = true;
    }

    // Establish (or re-establish) the connection outside the lock so a slow connect — or a
    // connect that fails because the database is down — does not block the whole pool. The slot
    // is already marked in_use, so no other worker touches it here. A connection that was closed
    // by a database restart is transparently replaced, which is how the pool self-heals.
    if (!chosen->conn || !chosen->conn->is_open()) {
        try {
            chosen->conn = open_connection();
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            chosen->in_use = false;
            cv_.notify_one();
            throw;
        }
    }
    return *chosen->conn;
}

pqxx::connection& ConnectionPool::refresh(pqxx::connection& conn) {
    Slot* target = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& slot : slots_) {
            if (slot.conn.get() == &conn) {
                target = &slot;
                break;
            }
        }
    }
    if (!target) throw std::runtime_error("Attempted to refresh unknown connection");
    // The slot is leased to the caller, so it is safe to rebuild its connection outside the lock.
    target->conn = open_connection();  // throws if the database is still unreachable
    return *target->conn;
}

void ConnectionPool::release(pqxx::connection& conn) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& slot : slots_) {
            if (slot.conn.get() == &conn) {
                slot.in_use = false;
                cv_.notify_one();
                return;
            }
        }
    }
    throw std::runtime_error("Attempted to release unknown connection");
}
