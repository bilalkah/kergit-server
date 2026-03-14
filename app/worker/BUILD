load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "worker_pool",
    srcs = [
        "WorkerPool.cpp",
    ],
    hdrs = [
        "WorkerPool.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:utils",
        "//app/dispatcher",
        "//app/managers/session:session_manager",
        "//app/queue:event_queue",
        "//core:server_config",
        "//domains/ids:domain_ids",
        "//net/outbound:i_outbound_sink",
        "//proto:envelope_cc_proto",
        "//proto:parsed_payload",
        "//proto/event:error_cc_proto",
        "//utils:loggable",
        "//utils:metrics",
    ],
)
