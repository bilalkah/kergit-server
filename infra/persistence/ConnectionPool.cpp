#include "infra/persistence/ConnectionPool.h"

#include <iostream>
ConnectionPool::ConnectionPool(const std::string& conninfo, std::size_t pool_size,
                               std::chrono::milliseconds wait_timeout)
    : wait_timeout_(wait_timeout) {
    if (pool_size == 0) throw std::invalid_argument("pool_size must be greater than zero");
    slots_.reserve(pool_size);
    for (std::size_t i = 0; i < pool_size; ++i) {
        auto connection = std::make_unique<pqxx::connection>(conninfo);
        if (!connection->is_open()) throw std::runtime_error("Failed to open database connection");
        slots_.push_back(Slot{std::move(connection), false});
    }
    std::cout << "ConnectionPool initialized with " << pool_size << " connections.\n";
}

pqxx::connection& ConnectionPool::acquire() {
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

    Slot* chosen = find_slot();
    while (!chosen) {
        if (!cv_.wait_for(lock, wait_timeout_, has_free_slot)) {
            throw std::runtime_error("Connection pool timeout waiting for available connection");
        }
        chosen = find_slot();
    }

    chosen->in_use = true;
    return *chosen->conn;
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
