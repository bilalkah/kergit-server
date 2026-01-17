load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "send_message_command",
    srcs = ["SendMessageCommand.cpp"],
    hdrs = ["SendMessageCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
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
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
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
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//domains:message_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:message_cc_proto",
        "//proto/domain:message_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:message_cc_proto",
    ],
)
