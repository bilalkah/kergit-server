#ifndef INFRA_PERSISTENCE_CONNECTION_POOL_H
#define INFRA_PERSISTENCE_CONNECTION_POOL_H

#include "utils/Loggable.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class ConnectionLease;

class ConnectionPool : public utils::Loggable {
   public:
    using Clock = std::chrono::system_clock;

    ConnectionPool(const std::string& conninfo, std::size_t pool_size,
                   std::chrono::milliseconds wait_timeout = std::chrono::milliseconds(5000));

    // Preferred API: borrow a connection as a scoped RAII lease that returns itself to the pool on
    // destruction. This is how callers should obtain a connection — the raw acquire/release pair
    // below exists only so ConnectionLease can implement it.
    ConnectionLease lease();

    pqxx::connection& acquire();
    void release(pqxx::connection& conn);
    // Reconnect a currently-leased connection that broke mid-use (e.g. the database restarted),
    // returning a reference to the fresh connection for the same slot. Throws if the database is
    // still unreachable.
    pqxx::connection& refresh(pqxx::connection& conn);

   private:
    // Open a brand-new connection from conninfo_. Throws if the database is unreachable.
    std::unique_ptr<pqxx::connection> open_connection() const;

    std::string conninfo_;

    // Slot storage is fixed at construction (never resized), so slot indices and the connection
    // pointers they hold stay stable for the pool's lifetime.
    std::vector<std::unique_ptr<pqxx::connection>> slots_;
    // Indices of slots not currently leased, used as a LIFO stack: acquire pops the back, release
    // pushes it back — both O(1), replacing the old linear scan for a free slot.
    std::vector<std::size_t> free_slots_;
    // Live connection pointer -> its slot index, so release()/refresh() locate a leased
    // connection's slot in O(1) instead of scanning every slot. Kept in lockstep with slots_:
    // whenever a slot's connection is (re)opened, the old pointer is erased and the new one added.
    std::unordered_map<const pqxx::connection*, std::size_t> conn_index_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::chrono::milliseconds wait_timeout_;
};

// RAII handle to a single borrowed connection. Owns exactly one responsibility: return the
// connection to the pool when it goes out of scope. reconnect() swaps in a fresh connection for the
// same slot when the current one broke mid-use. Move-only; never copied.
class ConnectionLease {
   public:
    ConnectionLease(ConnectionPool& pool, pqxx::connection& conn) : pool_(&pool), conn_(&conn) {}

    ConnectionLease(ConnectionLease&& other) noexcept : pool_(other.pool_), conn_(other.conn_) {
        other.pool_ = nullptr;
        other.conn_ = nullptr;
    }
    ConnectionLease& operator=(ConnectionLease&&) = delete;
    ConnectionLease(const ConnectionLease&) = delete;
    ConnectionLease& operator=(const ConnectionLease&) = delete;

    ~ConnectionLease() {
        if (pool_ && conn_) pool_->release(*conn_);
    }

    pqxx::connection& get() { return *conn_; }
    // Replace a connection that broke mid-use with a fresh one from the same slot. Throws if the
    // database is still unreachable.
    void reconnect() { conn_ = &pool_->refresh(*conn_); }

   private:
    ConnectionPool* pool_;
    pqxx::connection* conn_;
};

#endif  // INFRA_PERSISTENCE_CONNECTION_POOL_H
