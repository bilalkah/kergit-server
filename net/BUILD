load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "persocketdata",
    hdrs = [
        "PerSocketData.h",
    ],
    copts = ["-std=gnu++23"],
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
    copts = ["-std=gnu++23"],
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
    copts = ["-std=gnu++23"],
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
    copts = ["-std=gnu++23"],
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
    name = "outgoing_queue_consumer",
    srcs = ["OutgoingQueueConsumer.cpp"],
    hdrs = ["OutgoingQueueConsumer.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":client_gateway",
        ":connection_manager",
        "//app/queue:outgoing_queue",
        "//core:i_app",
        "//core:types",
        "//utils:loggable",
        "@uwebsockets",
    ],
)

cc_library(
    name = "websocket_server",
    srcs = ["WebSocketServer.cpp"],
    hdrs = ["WebSocketServer.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":client_gateway",
        ":connection_manager",
        ":heartbeat",
        ":outgoing_queue_consumer",
        ":persocketdata",
        "//app/queue:event_queue",
        "//app/queue:outgoing_queue",
        "//core:i_app",
        "//core:types",
        "//domains/ids:domain_ids",
        "@uwebsockets",
    ],
)
