load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "persistence_gateway_lib",
    srcs = [
        "ConnectionPool.cpp",
        "DatabaseExecutor.cpp",
        "PersistenceGateway.cpp",
        "repositories/ChannelRepository.cpp",
        "repositories/HubRepository.cpp",
        "repositories/UserRepository.cpp",
        "repositories/VoiceStateRepository.cpp",
    ],
    hdrs = [
        "ConnectionPool.h",
        "DatabaseExecutor.h",
        "PersistenceGateway.h",
        "RepositoryMux.h",
        "repositories/ChannelRepository.h",
        "repositories/HubRepository.h",
        "repositories/RepositoryUtils.h",
        "repositories/UserRepository.h",
        "repositories/VoiceStateRepository.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//domains:message_domain",
        "//domains:user_domain",
        "//domains/ids:domain_ids",
        "//utils:loggable",
        "@libpqxx",
        "@nlohmann_json",
    ],
)
