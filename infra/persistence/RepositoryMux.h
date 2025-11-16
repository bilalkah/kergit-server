#ifndef INFRA_PERSISTENCE_REPOSITORY_MUX_H
#define INFRA_PERSISTENCE_REPOSITORY_MUX_H

#include "infra/persistence/ConnectionPool.h"

#include <pqxx/pqxx>
#include <stdexcept>
#include <type_traits>

enum class Repository {
    Hub,
    Channel,
    Message,
    User,
};

class RepositoryMux {
   public:
    explicit RepositoryMux(ConnectionPool& pool) : pool_(pool) {}

    template <typename Fn>
    decltype(auto) run(Repository repository, Fn&& fn) {
        switch (repository) {
            case Repository::Hub:
            case Repository::Channel:
            case Repository::Message:
            case Repository::User:
                break;
            default:
                throw std::invalid_argument("Unsupported repository type");
        }

        class LeaseGuard {
           public:
            explicit LeaseGuard(ConnectionPool& pool) : pool_(pool), conn_(&pool.acquire()) {}
            ~LeaseGuard() {
                if (conn_) pool_.release(*conn_);
            }
            pqxx::connection& get() { return *conn_; }

           private:
            ConnectionPool& pool_;
            pqxx::connection* conn_;
        };

        LeaseGuard lease(pool_);
        pqxx::work txn(lease.get());
        using Result = std::invoke_result_t<Fn, pqxx::work&>;
        if constexpr (std::is_void_v<Result>) {
            std::forward<Fn>(fn)(txn);
            txn.commit();
        } else {
            auto result = std::forward<Fn>(fn)(txn);
            txn.commit();
            return result;
        }
    }

   private:
    ConnectionPool& pool_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORY_MUX_H
