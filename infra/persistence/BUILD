load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "persistence_gateway_lib",
    srcs = [
        "ConnectionPool.cpp",
        "PersistenceGateway.cpp",
        "repositories/ChannelRepository.cpp",
        "repositories/HubRepository.cpp",
        "repositories/UserRepository.cpp",
    ],
    hdrs = [
        "ConnectionPool.h",
        "PersistenceGateway.h",
        "RepositoryMux.h",
        "repositories/ChannelRepository.h",
        "repositories/HubRepository.h",
        "repositories/RepositoryUtils.h",
        "repositories/UserRepository.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/services:public_id_service",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//domains:message_domain",
        "//domains:user_domain",
        "//domains/ids:domain_ids",
        "@libpqxx",
        "@nlohmann_json",
    ],
)
