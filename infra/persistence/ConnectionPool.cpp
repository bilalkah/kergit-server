#include "infra/persistence/ConnectionPool.h"

#include "utils/Loggable.h"

ConnectionPool::ConnectionPool(const std::string& conninfo, std::size_t pool_size,
                               std::chrono::milliseconds wait_timeout)
    : conninfo_(conninfo), wait_timeout_(wait_timeout) {
    if (pool_size == 0) throw std::invalid_argument("pool_size must be greater than zero");
    slots_.resize(pool_size);  // slots start empty; connections are opened lazily in acquire()
    conn_index_.reserve(pool_size);
    free_slots_.reserve(pool_size);

    // Best-effort pre-warm. If the database is reachable we open the connections now so the first
    // requests are fast; if it is NOT reachable we DO NOT abort — the server stays up and each
    // slot connects on demand once the database returns. This is what lets the backend survive a
    // database that is down or restarting at startup.
    std::size_t opened = 0;
    for (std::size_t i = 0; i < pool_size; ++i) {
        try {
            slots_[i] = open_connection();
            conn_index_.emplace(slots_[i].get(), i);
            ++opened;
        } catch (const std::exception& e) {
            log(utils::LogLevel::WARN,
                "Database not reachable at startup; connections will be established lazily. "
                "Reason: ",
                e.what());
            break;  // stop hammering a down database; remaining slots connect on demand
        }
    }

    // Every slot starts free. Push indices in reverse so the back of the stack (popped first by
    // acquire) is slot 0 — i.e. the pre-warmed low-index slots are handed out before the lazy ones.
    for (std::size_t i = pool_size; i-- > 0;) {
        free_slots_.push_back(i);
    }

    log(utils::LogLevel::INFO, "Connection pool ready (", opened, "/", pool_size,
        " connections pre-warmed, rest lazy)");
}

ConnectionLease ConnectionPool::lease() { return ConnectionLease(*this, acquire()); }

std::unique_ptr<pqxx::connection> ConnectionPool::open_connection() const {
    auto connection = std::make_unique<pqxx::connection>(conninfo_);
    if (!connection->is_open()) throw std::runtime_error("Failed to open database connection");
    return connection;
}

pqxx::connection& ConnectionPool::acquire() {
    std::size_t index = 0;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, wait_timeout_, [&]() { return !free_slots_.empty(); })) {
            throw std::runtime_error("Connection pool timeout waiting for available connection");
        }
        index = free_slots_.back();
        free_slots_.pop_back();
    }

    // The slot is now leased to this caller (off the free list), so no other thread touches it.
    // Establish (or re-establish) the connection outside the lock so a slow connect — or one that
    // fails because the database is down — does not block the whole pool. A connection closed by a
    // database restart is transparently replaced here, which is how the pool self-heals.
    auto& slot = slots_[index];
    if (!slot || !slot->is_open()) {
        std::unique_ptr<pqxx::connection> fresh;
        try {
            fresh = open_connection();
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            free_slots_.push_back(index);
            cv_.notify_one();
            throw;
        }
        // Swap the new connection in and keep the index in lockstep, under the lock.
        std::lock_guard<std::mutex> lock(mutex_);
        if (slot) conn_index_.erase(slot.get());
        slot = std::move(fresh);
        conn_index_.emplace(slot.get(), index);
    }
    return *slot;
}

pqxx::connection& ConnectionPool::refresh(pqxx::connection& conn) {
    std::size_t index = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = conn_index_.find(&conn);
        if (it == conn_index_.end()) {
            throw std::runtime_error("Attempted to refresh unknown connection");
        }
        index = it->second;
    }

    // The slot is leased to the caller, so it is safe to rebuild its connection outside the lock.
    // open_connection() throws if the DB is still unreachable; we leave the slot/index untouched in
    // that case, so the caller's lease still maps to the (broken) connection and release() works.
    auto fresh = open_connection();

    std::lock_guard<std::mutex> lock(mutex_);
    conn_index_.erase(slots_[index].get());
    slots_[index] = std::move(fresh);
    conn_index_.emplace(slots_[index].get(), index);
    return *slots_[index];
}

void ConnectionPool::release(pqxx::connection& conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = conn_index_.find(&conn);
    if (it == conn_index_.end()) {
        throw std::runtime_error("Attempted to release unknown connection");
    }
    free_slots_.push_back(it->second);
    cv_.notify_one();
}
