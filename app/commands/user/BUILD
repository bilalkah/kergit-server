load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "update_user_command",
    srcs = ["UpdateUserCommand.cpp"],
    hdrs = ["UpdateUserCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//proto:envelope_cc_proto",
        "//proto/command:user_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)
