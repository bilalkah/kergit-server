#ifndef INFRA_PERSISTENCE_TRANSACTION_RUNNER_H
#define INFRA_PERSISTENCE_TRANSACTION_RUNNER_H

#include "infra/persistence/ConnectionPool.h"
#include "infra/persistence/PersistenceError.h"
#include "utils/Loggable.h"

#include <pqxx/pqxx>
#include <string_view>
#include <type_traits>
#include <utility>

// The one place that knows how to run a unit of database work and survive a transient connection
// break. Repositories hand it a function that takes a pqxx::work&; it leases a connection, runs the
// function inside a transaction, commits, and returns the result.
//
// Ownership is deliberately flat and explicit:
//   - ConnectionPool owns connection lifecycle (open / lend / return / replace).
//   - TransactionRunner owns transaction execution, retry-on-transient, and error classification.
//   - ConnectionLease (RAII) owns returning one borrowed connection to its pool.
// There is no other layer. "Who heals a dropped connection?" -> here, in run().
//
// Reads vs writes differ in exactly one way, the retry policy:
//   - read():  idempotent, so a dropped connection is always safe to retry.
//   - write(): a break *before* commit applied nothing and is safe to retry; a break *from*
//              commit() is left ambiguous, so it is not retried (never double-apply a mutation).
//
// Reads and writes also use separate pools (a bulkhead so a flood of slow reads cannot starve
// writes); the runner picks the pool for you.
class TransactionRunner : public utils::Loggable {
   public:
    TransactionRunner(ConnectionPool& read_pool, ConnectionPool& write_pool)
        : read_pool_(read_pool), write_pool_(write_pool) {}

    // Idempotent query. `tag` (e.g. "UserRepository.getUser") identifies the caller in retry logs.
    template <typename Fn>
    decltype(auto) read(std::string_view tag, Fn&& fn) {
        return run(read_pool_, /*idempotent=*/true, tag, std::forward<Fn>(fn));
    }

    // Mutating query. Not retried once commit() has been attempted.
    template <typename Fn>
    decltype(auto) write(std::string_view tag, Fn&& fn) {
        return run(write_pool_, /*idempotent=*/false, tag, std::forward<Fn>(fn));
    }

   private:
    template <typename Fn>
    decltype(auto) run(ConnectionPool& pool, bool idempotent, std::string_view tag, Fn&& fn) {
        using Result = std::invoke_result_t<Fn, pqxx::work&>;
        constexpr int kMaxAttempts = 2;

        // Borrowing a connection can fail if the database is down or the pool is momentarily
        // exhausted. Those are transient conditions the pool recovers from, so classify them.
        ConnectionLease lease = lease_or_transient(pool, tag);

        // `fn` is invoked as an lvalue (not std::forward) so it can be safely re-invoked on a
        // retry.
        for (int attempt = 1;; ++attempt) {
            // Tracks whether we reached commit() on this attempt. A broken connection before commit
            // applied nothing, so retrying is safe even for writes; only a break from commit()
            // itself is ambiguous.
            bool commit_attempted = false;
            try {
                pqxx::work txn(lease.get());
                if constexpr (std::is_void_v<Result>) {
                    fn(txn);
                    commit_attempted = true;
                    txn.commit();
                    return;
                } else {
                    auto result = fn(txn);
                    commit_attempted = true;
                    txn.commit();
                    return result;
                }
            } catch (const pqxx::broken_connection& ex) {
                // The connection dropped (DB restarted, or an idle pooled socket was killed
                // server-side — is_open() can't see this, so it only surfaces on first use).
                bool safe_to_retry = idempotent || !commit_attempted;
                if (safe_to_retry && attempt < kMaxAttempts) {
                    log(utils::LogLevel::WARN, tag,
                        ": connection dropped, reconnecting and retrying (attempt ", attempt + 1,
                        "/", kMaxAttempts, ")");
                    reconnect_or_transient(lease, tag);
                    continue;
                }
                // Out of safe retries: surface as transient so callers know the pool will heal and
                // the operation can be reattempted later.
                throw TransientPersistenceError(std::string(tag) +
                                                ": connection dropped: " + ex.what());
            }
            // Any other exception (e.g. a constraint violation) is a real failure and propagates
            // unchanged — it is never masked as transient.
        }
    }

    ConnectionLease lease_or_transient(ConnectionPool& pool, std::string_view tag) {
        try {
            return pool.lease();
        } catch (const std::exception& ex) {
            throw TransientPersistenceError(std::string(tag) +
                                            ": database unavailable: " + ex.what());
        }
    }

    void reconnect_or_transient(ConnectionLease& lease, std::string_view tag) {
        try {
            lease.reconnect();
        } catch (const std::exception& ex) {
            throw TransientPersistenceError(std::string(tag) +
                                            ": database unavailable: " + ex.what());
        }
    }

    ConnectionPool& read_pool_;
    ConnectionPool& write_pool_;
};

#endif  // INFRA_PERSISTENCE_TRANSACTION_RUNNER_H
