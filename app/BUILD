load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//app/commands:command_list.bzl", "COMMAND_LIST")

cc_library(
    name = "dispatcher",
    srcs = ["Dispatcher.cpp"],
    hdrs = ["Dispatcher.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "command_registry",
    srcs = ["CommandRegistery.cpp"],
    hdrs = [
        "CommandRegistery.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":dispatcher",
    ] + COMMAND_LIST,
)

