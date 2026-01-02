load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "disconnection_command",
    srcs = ["DisconnectionCommand.cpp"],
    hdrs = ["DisconnectionCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "@nlohmann_json",
    ],
)
