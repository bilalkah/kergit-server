#ifndef INFRA_PERSISTENCE_REPOSITORY_MUX_H
#define INFRA_PERSISTENCE_REPOSITORY_MUX_H

#include "infra/persistence/ConnectionPool.h"

#include <chrono>
#include <pqxx/pqxx>
#include <type_traits>
#include <utility>

struct DbExecTiming {
    std::chrono::steady_clock::time_point acquired_at{};
    std::chrono::steady_clock::time_point txn_start_at{};
    std::chrono::steady_clock::time_point txn_end_at{};
    const void* connection_id{nullptr};
    bool acquired{false};
    bool txn_started{false};
    bool txn_finished{false};
};

namespace detail {
class RepositoryMuxBase {
   public:
    explicit RepositoryMuxBase(ConnectionPool& pool) : pool_(pool) {}

    template <typename Fn>
    decltype(auto) run(Fn&& fn, DbExecTiming* timing = nullptr) {
        class LeaseGuard {
           public:
            explicit LeaseGuard(ConnectionPool& pool, DbExecTiming* timing)
                : pool_(pool), conn_(&pool.acquire()), timing_(timing) {
                if (timing_) {
                    timing_->acquired = true;
                    timing_->acquired_at = std::chrono::steady_clock::now();
                    timing_->connection_id = conn_;
                }
            }
            ~LeaseGuard() {
                if (conn_) pool_.release(*conn_);
            }
            pqxx::connection& get() { return *conn_; }

           private:
            ConnectionPool& pool_;
            pqxx::connection* conn_;
            DbExecTiming* timing_;
        };

        LeaseGuard lease(pool_, timing);
        pqxx::work txn(lease.get());
        if (timing) {
            timing->txn_started = true;
            timing->txn_start_at = std::chrono::steady_clock::now();
        }
        using Result = std::invoke_result_t<Fn, pqxx::work&>;
        try {
            if constexpr (std::is_void_v<Result>) {
                std::forward<Fn>(fn)(txn);
                txn.commit();
                if (timing) {
                    timing->txn_finished = true;
                    timing->txn_end_at = std::chrono::steady_clock::now();
                }
            } else {
                auto result = std::forward<Fn>(fn)(txn);
                txn.commit();
                if (timing) {
                    timing->txn_finished = true;
                    timing->txn_end_at = std::chrono::steady_clock::now();
                }
                return result;
            }
        } catch (...) {
            if (timing) {
                timing->txn_finished = true;
                timing->txn_end_at = std::chrono::steady_clock::now();
            }
            throw;
        }
    }

   private:
    ConnectionPool& pool_;
};
}  // namespace detail

class ReadRepositoryMux {
   public:
    explicit ReadRepositoryMux(ConnectionPool& pool) : mux_(pool) {}

    template <typename Fn>
    decltype(auto) run(Fn&& fn, DbExecTiming* timing = nullptr) {
        return mux_.run(std::forward<Fn>(fn), timing);
    }

   private:
    detail::RepositoryMuxBase mux_;
};

class WriteRepositoryMux {
   public:
    explicit WriteRepositoryMux(ConnectionPool& pool) : mux_(pool) {}

    template <typename Fn>
    decltype(auto) run(Fn&& fn, DbExecTiming* timing = nullptr) {
        return mux_.run(std::forward<Fn>(fn), timing);
    }

   private:
    detail::RepositoryMuxBase mux_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORY_MUX_H
