load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "auth_command",
    srcs = ["AuthCommand.cpp"],
    hdrs = [
        "AuthCommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
    ],
)
