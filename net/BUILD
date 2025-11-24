load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "persocketdata",
    hdrs = [
        "PerSocketData.h",
    ],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        "//domains:hub_domain",
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "connection_manager",
    srcs = ["ConnectionManager.cpp"],
    hdrs = ["ConnectionManager.h"],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/pubsub",
        "//core:types",
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "heartbeat",
    srcs = ["Heartbeat.cpp"],
    hdrs = ["Heartbeat.h"],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        ":connection_manager",
        ":persocketdata",
        "//core:i_app",
        "//core:types",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)

cc_library(
    name = "client_gateway",
    srcs = ["ClientGateway.cpp"],
    hdrs = ["ClientGateway.h"],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        ":connection_manager",
        "//core:i_app",
        "//core:types",
        "//domains/ids:domain_ids",
        "//utils:loggable",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "websocket_server",
    srcs = ["WebSocketServer.cpp"],
    hdrs = ["WebSocketServer.h"],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        ":client_gateway",
        ":connection_manager",
        ":heartbeat",
        ":persocketdata",
        "//app:dispatcher",
        "//app/queue:ts_cmd_queue",
        "//app/services:hub_publisher",
        "//core:i_app",
        "//core:types",
        "//domains/ids:domain_ids",
        "//infra/security:message_validator",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)
