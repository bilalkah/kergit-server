load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "outbound_msg",
    hdrs = ["Msg.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "outgoing_queue",
    hdrs = ["OutgoingQueue.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        ":outbound_msg",
        "//core/base:ts_queue",
    ],
)

cc_library(
    name = "outgoing_worker",
    srcs = ["OutgoingWorker.cpp"],
    hdrs = [
        "IOutBoundSink.h",
        "OutgoingWorker.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = [
        "//net:__subpackages__",
    ],
    deps = [
        ":outgoing_queue",
        "//net/connection:connection_registery",
        "//net/transport:i_loop",
        "//utils:loggable",
        "@uwebsockets",
    ],
)
