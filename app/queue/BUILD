load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "ts_cmd_queue",
    hdrs = [
        "ThreadSafeCmdQueue.h",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "event_queue",
    hdrs = ["EventQueue.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ts_cmd_queue",
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "outgoing_queue",
    hdrs = ["OutGoingQueue.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ts_cmd_queue",
        "//app/commands:i_command",
        "//core:i_app",
        "//net:client_gateway",
        "//net:connection_manager",
        "@nlohmann_json",
    ],
)
