#ifndef INFRA_PERSISTENCE_CONNECTION_POOL_H
#define INFRA_PERSISTENCE_CONNECTION_POOL_H

#include "utils/Loggable.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <stdexcept>
#include <string>
#include <vector>

class ConnectionPool : public utils::Loggable {
   public:
    using Clock = std::chrono::system_clock;

    ConnectionPool(const std::string& conninfo, std::size_t pool_size,
                   std::chrono::milliseconds wait_timeout = std::chrono::milliseconds(5000));

    pqxx::connection& acquire();
    void release(pqxx::connection& conn);
    // Reconnect a currently-leased connection that broke mid-use (e.g. the database restarted),
    // returning a reference to the fresh connection for the same slot. Throws if the database is
    // still unreachable.
    pqxx::connection& refresh(pqxx::connection& conn);

   private:
    struct Slot {
        std::unique_ptr<pqxx::connection> conn;
        bool in_use{false};
    };

    // Open a brand-new connection from conninfo_. Throws if the database is unreachable.
    std::unique_ptr<pqxx::connection> open_connection() const;

    std::string conninfo_;
    std::vector<Slot> slots_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::chrono::milliseconds wait_timeout_;
};

#endif  // INFRA_PERSISTENCE_CONNECTION_POOL_H
