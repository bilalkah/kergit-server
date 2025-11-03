load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "persocketdata",
    hdrs = [
        "PerSocketData.h",
    ],
    visibility = ["//visibility:public"],
    deps = ["//domains/ids:domain_ids"],
)

cc_library(
    name = "connection_manager",
    srcs = ["ConnectionManager.cpp"],
    hdrs = ["ConnectionManager.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/pubsub",
        "//core:types",
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "client_gateway",
    srcs = ["ClientGateway.cpp"],
    hdrs = ["ClientGateway.h"],
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
    visibility = ["//visibility:public"],
    deps = [
        ":client_gateway",
        ":connection_manager",
        ":persocketdata",
        "//app:dispatcher",
        "//core:i_app",
        "//core:types",
        "//domains/ids:domain_ids",
        "@nlohmann_json",
        "@uwebsockets",  # or uwebsockets depending on kSslEnabled
    ],
)
