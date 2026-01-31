load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "event_msg",
    hdrs = ["Msg.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
        "//proto:envelope_cc_proto",
    ],
)

cc_library(
    name = "i_event_sink",
    hdrs = ["IEventSink.h"],
    copts = ["-std=gnu++23"],
    visibility = [
        "//app:__subpackages__",
        "//net:__subpackages__",
    ],
    deps = [
        ":event_msg",
    ],
)

cc_library(
    name = "event_queue",
    hdrs = [
        "EventQueue.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = [
        "//app:__subpackages__",
    ],
    deps = [
        ":event_msg",
        ":i_event_sink",
        "//utils:metrics",
    ],
)
