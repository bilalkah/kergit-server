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
    name = "ws_adapter",
    hdrs = [
        "WSAdapter.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//core:i_websocket",
        "//core:types",
    ],
)

cc_library(
    name = "connection_manager",
    srcs = ["ConnectionManager.cpp"],
    hdrs = ["ConnectionManager.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//core:i_websocket",
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "websocket_server",
    srcs = ["WebSocketServer.cpp"],
    hdrs = ["WebSocketServer.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app:dispatcher",
        "//core:i_app",
        "//core:i_websocket",
        "//core:types",
        "//domains/ids:domain_ids",
        "//net:connection_manager",
        "//net:ws_adapter",
        "@nlohmann_json",
        "@uwebsockets",  # or uwebsockets depending on kSslEnabled
    ],
)
