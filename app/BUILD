load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "i_command",
    hdrs = [
        "ICommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "dispatcher",
    hdrs = ["Dispatcher.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "command_registry",
    hdrs = [
        "CommandRegistery.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":dispatcher",
        "//app/commands:ping_command",
    ],
)
