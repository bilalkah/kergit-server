load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "worker_pool",
    srcs = [
        "WorkerPool.cpp",
    ],
    hdrs = [
        "WorkerPool.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app:dispatcher",
        "//app/queue:event_queue",
        "//app/queue:outgoing_queue",
        "//domains/ids:domain_ids",
        "//infra/security:message_validator",
        "//utils:loggable",
        "@nlohmann_json",
    ],
)
