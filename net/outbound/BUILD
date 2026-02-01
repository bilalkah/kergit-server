load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "outbound_msg",
    hdrs = ["Msg.h"],
    copts = ["-std=gnu++23"],
    visibility = [
        "//app/commands:__subpackages__",
        "//net:__subpackages__",
    ],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "outbound_flush_engine",
    srcs = ["OutboundFlushEngine.cpp"],
    hdrs = ["OutboundFlushEngine.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//net/connection:connection_registery",
        "//net/transport:outbound_transport_iface",
        "//net/transport/websocket:uws_types",
        "//utils:metrics",
    ],
)

cc_library(
    name = "outgoing_queue",
    hdrs = ["OutgoingQueue.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        ":outbound_msg",
        "//utils:metrics",
    ],
)

cc_library(
    name = "i_outbound_sink",
    hdrs = ["IOutBoundSink.h"],
    copts = ["-std=gnu++23"],
    visibility = [
        "//app:__subpackages__",
        "//net:__subpackages__",
    ],
    deps = [
        ":outbound_msg",
    ],
)

cc_library(
    name = "outgoing_worker",
    srcs = ["OutgoingWorker.cpp"],
    hdrs = [
        "OutgoingWorker.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = [
        "//net:__subpackages__",
    ],
    deps = [
        ":outgoing_queue",
        "//net/connection:connection_registery",
        "//net/outbound:i_outbound_sink",
        "//net/transport:i_loop",
        "//utils:loggable",
        "//utils:metrics",
        "@uwebsockets",
    ],
)
