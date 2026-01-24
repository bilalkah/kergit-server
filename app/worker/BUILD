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
        "//app/dispatcher",
        "//app/managers/session:session_manager",
        "//app/queue:event_queue",
        "//core:server_config",
        "//domains/ids:domain_ids",
        "//infra/security:message_validator",
        "//infra/security:proto_message_validator",
        "//net/outbound:i_outbound_sink",
        "//proto:envelope_cc_proto",
        "//proto/event:error_cc_proto",
        "//utils:loggable",
        "@nlohmann_json",
    ],
)
