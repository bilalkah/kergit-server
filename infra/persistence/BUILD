load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "persistence_gateway_lib",
    srcs = [
        "ConnectionPool.cpp",
        "PersistenceGateway.cpp",
        "repositories/HubRepository.cpp",
        "repositories/MessageRepository.cpp",
        "repositories/UserRepository.cpp",
        "repositories/AuditRepository.cpp",
    ],
    hdrs = [
        "ConnectionPool.h",
        "PersistenceError.h",
        "PersistenceGateway.h",
        "TransactionRunner.h",
        "repositories/HubRepository.h",
        "repositories/MessageRepository.h",
        "repositories/RepositoryUtils.h",
        "repositories/UserRepository.h",
        "repositories/RepositorySchema.h",
        "repositories/AuditRepository.h",
    ],
    copts = [
        # libpqxx's pqxx::params::append (params.hxx) trips a GCC-13 -Wmaybe-uninitialized
        # false positive on its internal std::variant when inlined at -O2 (opt builds only).
        # The flagged code is in the third-party header, not ours, and the warning cannot be
        # suppressed at the call site (it is anchored to the libpqxx header post-inlining), so
        # disable it for this library — the only consumer of libpqxx's param binding.
        "-Wno-maybe-uninitialized",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//domains:message_domain",
        "//domains:user_domain",
        "//domains/ids:domain_ids",
        "//core:server_config",
        "//utils:loggable",
        "@libpqxx",
        "@nlohmann_json",
    ],
)
