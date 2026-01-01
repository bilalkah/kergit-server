load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "event_msg",
    hdrs = ["Msg.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "event_queue",
    hdrs = [
        "EventQueue.h",
        "IEventSink.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = [
        "//app:__subpackages__",
        "//net:__subpackages__",
    ],
    deps = [
        ":event_msg",
        "//core/base:ts_queue",
    ],
)
