load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "select_active_channel_command",
    srcs = ["SelectActiveChannelCommand.cpp"],
    hdrs = ["SelectActiveChannelCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:message_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:activity_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "typing_command",
    srcs = ["TypingCommand.cpp"],
    hdrs = ["TypingCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//proto:envelope_cc_proto",
        "//proto/command:activity_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:realtime_cc_proto",
    ],
)

cc_library(
    name = "join_voice_channel_command",
    srcs = ["JoinVoiceChannelCommand.cpp"],
    hdrs = ["JoinVoiceChannelCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:message_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:activity_cc_proto",
        "//proto/event:activity_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "voice_channel_activity_command",
    srcs = ["VoiceChannelActivityCommand.cpp"],
    hdrs = ["VoiceChannelActivityCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//proto:envelope_cc_proto",
        "//proto/command:activity_cc_proto",
        "//proto/event:activity_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)
