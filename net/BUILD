load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "net_types",
    hdrs = ["Types.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "network_stack",
    srcs = ["NetworkStack.cpp"],
    hdrs = ["NetworkStack.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//app/queue:event_queue",
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
    visibility = ["//app:__subpackages__"],
    deps = [
        ":network_stack",
        "//domains/ids:domain_ids",
        "//net/outbound:outgoing_queue",
    ],
)
