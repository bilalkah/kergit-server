load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "bootstrap_command",
    srcs = ["BootstrapCommand.cpp"],
    hdrs = [
        "BootstrapCommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/converters:proto_converters",
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/event:bootstrap_cc_proto",
        "@nlohmann_json",
    ],
)
