#ifndef INFRA_PERSISTENCE_DATABASE_EXECUTOR_H
#define INFRA_PERSISTENCE_DATABASE_EXECUTOR_H

#include "infra/persistence/RepositoryMux.h"
#include "utils/Logger.h"

#include <chrono>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

struct DbRequestContext {
    std::string_view request_id{};
    std::string_view caller_tag{};
};

class DatabaseExecutor {
   public:
    DatabaseExecutor(ReadRepositoryMux& read_mux, WriteRepositoryMux& write_mux)
        : read_mux_(read_mux), write_mux_(write_mux) {}

    template <typename Fn>
    decltype(auto) read(std::string_view caller_tag, Fn&& fn) {
        DbRequestContext ctx{.request_id = {}, .caller_tag = caller_tag};
        return read(ctx, std::forward<Fn>(fn));
    }

    template <typename Fn>
    decltype(auto) read(const DbRequestContext& ctx, Fn&& fn) {
        return execute("read", ctx, read_mux_, std::forward<Fn>(fn));
    }

    template <typename Fn>
    decltype(auto) write(std::string_view caller_tag, Fn&& fn) {
        DbRequestContext ctx{.request_id = {}, .caller_tag = caller_tag};
        return write(ctx, std::forward<Fn>(fn));
    }

    template <typename Fn>
    decltype(auto) write(const DbRequestContext& ctx, Fn&& fn) {
        return execute("write", ctx, write_mux_, std::forward<Fn>(fn));
    }

   private:
    static long long to_ms(std::chrono::steady_clock::duration duration) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    static void log_exec(const char* op, const DbRequestContext& ctx, const DbExecTiming& timing,
                         long long wait_ms, long long exec_ms, long long total_ms,
                         const char* status, const char* error) {
        std::ostringstream oss;
        oss << "db_exec op=" << op;
        oss << " repo=" << (ctx.caller_tag.empty() ? "unknown" : ctx.caller_tag);
        oss << " request_id=" << (ctx.request_id.empty() ? "unknown" : ctx.request_id);
        if (timing.connection_id) {
            oss << " conn_id=" << timing.connection_id;
        } else {
            oss << " conn_id=unknown";
        }
        oss << " wait_ms=" << wait_ms;
        oss << " exec_ms=" << exec_ms;
        oss << " total_ms=" << total_ms;
        oss << " status=" << status;
        if (error) {
            oss << " error=" << error;
        }
        utils::log_line(utils::LogLevel::INFO, oss.str());
    }

    template <typename Mux, typename Fn>
    decltype(auto) execute(const char* op, const DbRequestContext& ctx, Mux& mux, Fn&& fn) {
        const auto started_at = std::chrono::steady_clock::now();
        DbExecTiming timing{};
        auto finalize = [&](const char* status, const char* error, std::chrono::steady_clock::time_point end_at) {
            const auto wait_ms =
                timing.acquired ? to_ms(timing.acquired_at - started_at) : to_ms(end_at - started_at);
            const auto exec_ms = (timing.txn_started && timing.txn_finished)
                                     ? to_ms(timing.txn_end_at - timing.txn_start_at)
                                     : 0;
            const auto total_ms = to_ms(end_at - started_at);
            log_exec(op, ctx, timing, wait_ms, exec_ms, total_ms, status, error);
        };

        try {
            using Result = std::invoke_result_t<Fn, pqxx::work&>;
            if constexpr (std::is_void_v<Result>) {
                mux.run(std::forward<Fn>(fn), &timing);
                const auto end_at = timing.txn_finished ? timing.txn_end_at
                                                        : std::chrono::steady_clock::now();
                finalize("ok", nullptr, end_at);
            } else {
                auto result = mux.run(std::forward<Fn>(fn), &timing);
                const auto end_at = timing.txn_finished ? timing.txn_end_at
                                                        : std::chrono::steady_clock::now();
                finalize("ok", nullptr, end_at);
                return result;
            }
        } catch (const std::exception& ex) {
            const auto end_at =
                timing.txn_finished ? timing.txn_end_at : std::chrono::steady_clock::now();
            finalize("exception", typeid(ex).name(), end_at);
            throw;
        } catch (...) {
            const auto end_at =
                timing.txn_finished ? timing.txn_end_at : std::chrono::steady_clock::now();
            finalize("exception", "unknown", end_at);
            throw;
        }
    }

    ReadRepositoryMux& read_mux_;
    WriteRepositoryMux& write_mux_;
};

#endif  // INFRA_PERSISTENCE_DATABASE_EXECUTOR_H
