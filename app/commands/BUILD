load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "i_command",
    hdrs = [
        "ICommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "//net:persocketdata",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "ping_command",
    hdrs = [
        "PingCommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)

cc_library(
    name = "auth_command",
    srcs = ["AuthCommand.cpp"],
    hdrs = [
        "AuthCommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/services:auth_service",
        "//app/services:hub_publisher",
        "//infra/persistence:chatdb_lib",
        "//net:client_gateway",
        "//net:connection_manager",
    ],
)
