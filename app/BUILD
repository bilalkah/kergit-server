load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "app_stack",
    srcs = ["AppStack.cpp"],
    hdrs = ["AppStack.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/dispatcher",
        "//app/queue:event_queue",
        "//app/services/invite:invite_service",
        "//app/services/voice:voice_service",
        "//app/worker:worker_pool",
        "//core:server_config",
        "//infra/persistence:persistence_gateway_lib",
        "//infra/redis:redis_client",
        "//livekit/webhook:livekit_webhook_server",
        "//net/outbound:i_outbound_sink",
        "//utils:env_loader",
        "//utils:loggable",
    ],
)
