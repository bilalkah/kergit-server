load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "send_message_command",
    srcs = ["SendMessageCommand.cpp"],
    hdrs = ["SendMessageCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//app/proto_builders",
        "//domains:message_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:message_cc_proto",
        "//proto/domain:message_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:message_cc_proto",
    ],
)

cc_library(
    name = "fetch_latest_messages_command",
    srcs = ["FetchLatestMessagesCommand.cpp"],
    hdrs = ["FetchLatestMessagesCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/proto_builders",
        "//domains:message_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:message_cc_proto",
        "//proto/domain:message_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:message_cc_proto",
    ],
)

cc_library(
    name = "fetch_messages_before_command",
    srcs = ["FetchMessagesBeforeCommand.cpp"],
    hdrs = ["FetchMessagesBeforeCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/proto_builders",
        "//domains:message_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:message_cc_proto",
        "//proto/domain:message_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:message_cc_proto",
    ],
)
