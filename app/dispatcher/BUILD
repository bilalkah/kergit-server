load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//app/commands:command_list.bzl", "COMMAND_LIST")

cc_library(
    name = "dispatcher",
    srcs = ["Dispatcher.cpp"],
    hdrs = [
        "CommandContext.h",
        "Dispatcher.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "@nlohmann_json",
    ] + COMMAND_LIST,
)
