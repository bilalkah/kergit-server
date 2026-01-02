load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "app_stack",
    srcs = ["AppStack.cpp"],
    hdrs = ["AppStack.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/dispatcher",
        "//app/queue:event_queue",
        "//app/services:public_id_service",
        "//app/services/auth:auth_service",
        "//app/services/channel:channel_service",
        "//app/services/hub:hub_notifier",
        "//app/services/hub:hub_service",
        "//app/services/hub:hub_snapshot_builder",
        "//app/services/presence:presence_service",
        "//app/services/user:user_service",
        "//app/worker:worker_pool",
        "//core:server_config",
        "//infra/persistence:persistence_gateway_lib",
        "//infra/security:message_validator",
        "//net/outbound:i_outbound_sink",
        "//utils:loggable",
    ],
)
