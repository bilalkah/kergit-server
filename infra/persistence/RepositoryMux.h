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
    decltype(auto) run(Fn&& fn, DbExecTiming* timing = nullptr, bool allow_retry = false) {
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
            // Replace a connection that broke mid-use with a fresh one from the same slot.
            void reconnect() {
                conn_ = &pool_.refresh(*conn_);
                if (timing_) timing_->connection_id = conn_;
            }

           private:
            ConnectionPool& pool_;
            pqxx::connection* conn_;
            DbExecTiming* timing_;
        };

        LeaseGuard lease(pool_, timing);
        using Result = std::invoke_result_t<Fn, pqxx::work&>;
        constexpr int kMaxAttempts = 2;

        // `fn` is invoked as an lvalue (not std::forward) so it is safe to call again on a retry.
        for (int attempt = 1;; ++attempt) {
            // Tracks whether we got as far as calling commit() on this attempt. A broken
            // connection thrown *before* commit means the transaction never reached the server's
            // commit phase, so nothing was applied and retrying is always safe — even for writes.
            // Only a break thrown *from* commit() leaves the outcome genuinely ambiguous.
            bool commit_attempted = false;
            try {
                pqxx::work txn(lease.get());
                if (timing) {
                    timing->txn_started = true;
                    timing->txn_start_at = std::chrono::steady_clock::now();
                }
                if constexpr (std::is_void_v<Result>) {
                    fn(txn);
                    commit_attempted = true;
                    txn.commit();
                    if (timing) {
                        timing->txn_finished = true;
                        timing->txn_end_at = std::chrono::steady_clock::now();
                    }
                    return;
                } else {
                    auto result = fn(txn);
                    commit_attempted = true;
                    txn.commit();
                    if (timing) {
                        timing->txn_finished = true;
                        timing->txn_end_at = std::chrono::steady_clock::now();
                    }
                    return result;
                }
            } catch (const pqxx::broken_connection&) {
                if (timing) {
                    timing->txn_finished = true;
                    timing->txn_end_at = std::chrono::steady_clock::now();
                }
                // The database connection dropped (e.g. the DB restarted), most commonly because a
                // connection that had been sitting idle in the pool was killed server-side —
                // is_open() can't detect this, so the breakage only surfaces on the first query.
                //
                // Reconnect and retry when it is safe to do so:
                //   - reads are idempotent, so allow_retry lets them retry through any break;
                //   - writes are not auto-retried *after* commit() (avoid double-applying a
                //     mutation), but a break *before* commit() applied nothing, so retrying it is
                //     safe and lets the first write after a DB blip heal instead of failing.
                // Either way the pool heals the broken connection (here, or on the next
                // acquire()), so the server recovers automatically once the database is back.
                bool safe_to_retry = allow_retry || !commit_attempted;
                if (!safe_to_retry || attempt >= kMaxAttempts) throw;
                lease.reconnect();  // throws if the DB is still unreachable -> caught upstream
            } catch (...) {
                if (timing) {
                    timing->txn_finished = true;
                    timing->txn_end_at = std::chrono::steady_clock::now();
                }
                throw;
            }
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
        // Reads are idempotent: reconnect-and-retry once on a dropped connection.
        return mux_.run(std::forward<Fn>(fn), timing, /*allow_retry=*/true);
    }

   private:
    detail::RepositoryMuxBase mux_;
};

class WriteRepositoryMux {
   public:
    explicit WriteRepositoryMux(ConnectionPool& pool) : mux_(pool) {}

    template <typename Fn>
    decltype(auto) run(Fn&& fn, DbExecTiming* timing = nullptr) {
        // Writes are not auto-retried (avoid double-applying a mutation); the pool still heals
        // the broken connection on the next acquire().
        return mux_.run(std::forward<Fn>(fn), timing, /*allow_retry=*/false);
    }

   private:
    detail::RepositoryMuxBase mux_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORY_MUX_H
