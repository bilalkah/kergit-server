load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

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
    name = "reliable_seq",
    hdrs = ["ReliableSeq.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//proto:envelope_cc_proto",
    ],
)

cc_test(
    name = "reliable_seq_test",
    srcs = ["ReliableSeq_test.cc"],
    copts = ["-std=gnu++23"],
    deps = [
        ":reliable_seq",
        "//proto:envelope_cc_proto",
        "@googletest//:gtest_main",
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
        ":reliable_seq",
        "//net/connection:connection_registery",
        "//net/outbound:i_outbound_sink",
        "//net/transport:outbound_transport_iface",
        "//net/transport:i_loop",
        "//utils:loggable",
        "//utils:metrics",
        "@uwebsockets",
    ],
)
