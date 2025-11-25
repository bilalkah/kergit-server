load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "ts_cmd_queue",
    hdrs = [
        "ThreadSafeCmdQueue.h",
    ],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "event_queue",
    hdrs = ["EventQueue.h"],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        ":ts_cmd_queue",
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "outgoing_queue",
    hdrs = ["OutgoingQueue.h"],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        ":ts_cmd_queue",
        "//domains/ids:domain_ids",
        "@nlohmann_json",
    ],
)
