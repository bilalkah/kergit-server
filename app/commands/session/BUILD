load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "bootstrap_command",
    srcs = ["BootstrapCommand.cpp"],
    hdrs = [
        "BootstrapCommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/converters:proto_converters",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//app/proto_builders",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:presence_cc_proto",
        "//proto/event:session_cc_proto",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "authenticate_command",
    srcs = ["AuthenticateCommand.cpp"],
    hdrs = [
        "AuthenticateCommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/proto_builders",
        "//proto:envelope_cc_proto",
        "//proto/command:session_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:session_cc_proto",
    ],
)
