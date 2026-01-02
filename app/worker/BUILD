load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "worker_pool",
    srcs = [
        "AuthGuard.cpp",
        "WorkerPool.cpp",
    ],
    hdrs = [
        "AuthGuard.h",
        "WorkerPool.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/dispatcher",
        "//app/queue:event_queue",
        "//app/managers/session:session_manager",
        "//core:server_config",
        "//domains/ids:domain_ids",
        "//infra/security:message_validator",
        "//net/outbound:i_outbound_sink",
        "//utils:loggable",
        "@nlohmann_json",
    ],
)
