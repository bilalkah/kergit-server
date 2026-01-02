load("@rules_cc//cc:cc_library.bzl", "cc_library")

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
        "//app/queue:event_queue",
        "//core:server_config",
        "//domains/ids:domain_ids",
        "//infra/security:message_validator",
        "//net/outbound:outgoing_worker",
        "//utils:loggable",
        "@nlohmann_json",
    ],
)
