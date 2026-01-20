load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "net_types",
    hdrs = ["Types.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
)

cc_library(
    name = "network_stack",
    srcs = ["NetworkStack.cpp"],
    hdrs = ["NetworkStack.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//server:__subpackages__"],
    deps = [
        "//app/queue:i_event_sink",
        "//net/connection:connection_registery",
        "//net/outbound:outgoing_queue",
        "//net/runtime:heartbeat_service",
        "//net/transport/websocket:text_websocket_transport",
        "//utils:loggable",
    ],
)

cc_library(
    name = "network_router",
    srcs = ["NetworkRouter.cpp"],
    hdrs = ["NetworkRouter.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//server:__subpackages__"],
    deps = [
        ":network_stack",
        "//domains/ids:domain_ids",
        "//net/outbound:i_outbound_sink",
        "//net/outbound:outgoing_queue",
    ],
)
