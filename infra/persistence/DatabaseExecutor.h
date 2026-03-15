#ifndef INFRA_PERSISTENCE_DATABASE_EXECUTOR_H
#define INFRA_PERSISTENCE_DATABASE_EXECUTOR_H

#include "infra/persistence/RepositoryMux.h"

#include <chrono>
#include <string_view>
#include <type_traits>
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
    template <typename Mux, typename Fn>
    decltype(auto) execute(const char* op, const DbRequestContext& ctx, Mux& mux, Fn&& fn) {
        (void)op;
        (void)ctx;
        return mux.run(std::forward<Fn>(fn));
    }

    ReadRepositoryMux& read_mux_;
    WriteRepositoryMux& write_mux_;
};

#endif  // INFRA_PERSISTENCE_DATABASE_EXECUTOR_H
