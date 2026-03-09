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
        "//app/services/voice:voice_service",
        "//core:server_config",
        "//infra/persistence:persistence_gateway_lib",
        "//net/outbound:i_outbound_sink",
        "//utils:env_loader",
        "//utils:loggable",
        "//livekit/webhook:livekit_webhook_server",
    ],
)
