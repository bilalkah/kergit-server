#ifndef INFRA_PERSISTENCE_CONNECTION_POOL_H
#define INFRA_PERSISTENCE_CONNECTION_POOL_H

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <stdexcept>
#include <string>
#include <vector>

class ConnectionPool {
   public:
    using Clock = std::chrono::steady_clock;

    ConnectionPool(const std::string& conninfo, std::size_t pool_size,
                   std::chrono::milliseconds wait_timeout = std::chrono::milliseconds(5000));

    pqxx::connection& acquire();
    void release(pqxx::connection& conn);

   private:
    struct Slot {
        std::unique_ptr<pqxx::connection> conn;
        bool in_use{false};
    };

    std::vector<Slot> slots_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::chrono::milliseconds wait_timeout_;
};

#endif  // INFRA_PERSISTENCE_CONNECTION_POOL_H
