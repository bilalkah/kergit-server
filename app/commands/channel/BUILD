load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "create_channel_command",
    srcs = ["CreateChannelCommand.cpp"],
    hdrs = ["CreateChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/converters:proto_converters",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:channel_cc_proto",
        "//proto/event:channel_cc_proto",
        "//proto/event:error_cc_proto",
    ],
)

cc_library(
    name = "rename_channel_command",
    srcs = ["RenameChannelCommand.cpp"],
    hdrs = ["RenameChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/converters:proto_converters",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:channel_cc_proto",
        "//proto/event:channel_cc_proto",
        "//proto/event:error_cc_proto",
    ],
)
