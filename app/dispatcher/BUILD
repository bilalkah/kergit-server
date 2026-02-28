load("@rules_cc//cc:defs.bzl", "cc_library")
load("//app/commands:command_list.bzl", "COMMAND_LIST")

cc_library(
    name = "command_context",
    hdrs = ["CommandContext.h"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//app/managers/session:session_manager",
        "//app/managers/subscription:subscription_manager",
        "//app/queue:i_event_sink",
        "//app/services/auth:auth_service",
        "//app/services/channel:channel_service",
        "//app/services/hub:hub_notifier",
        "//app/services/hub:hub_service",
        "//app/services/hub:hub_snapshot_builder",
        "//app/services/livekit:livekit_token_service",
        "//app/services/presence:presence_service",
        "//app/services/user:user_service",
    ],
)

cc_library(
    name = "dispatcher",
    srcs = ["Dispatcher.cpp"],
    hdrs = [
        "CommandContext.h",
        "Dispatcher.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":command_context",
        "//app/commands:i_command",
        "//proto:envelope_cc_proto",
        "//utils:loggable",
        "@nlohmann_json",
    ] + COMMAND_LIST,
)
