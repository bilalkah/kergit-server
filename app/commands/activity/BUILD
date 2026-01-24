load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "select_active_channel_command",
    srcs = ["SelectActiveChannelCommand.cpp"],
    hdrs = ["SelectActiveChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//proto:envelope_cc_proto",
        "//proto/command:activity_cc_proto",
        "//proto/event:error_cc_proto",
    ],
)

cc_library(
    name = "typing_command",
    srcs = ["TypingCommand.cpp"],
    hdrs = ["TypingCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//proto:envelope_cc_proto",
        "//proto/command:activity_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:presence_cc_proto",
    ],
)
