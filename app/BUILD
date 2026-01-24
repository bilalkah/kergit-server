load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "app_stack",
    srcs = ["AppStack.cpp"],
    hdrs = ["AppStack.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/dispatcher",
        "//app/queue:event_queue",
        "//app/worker:worker_pool",
        "//core:server_config",
        "//infra/persistence:persistence_gateway_lib",
        "//infra/security:message_validator",
        "//net/outbound:i_outbound_sink",
        "//utils:loggable",
    ],
)
