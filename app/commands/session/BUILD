load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "state_sync_builder",
    srcs = ["StateSyncBuilder.cpp"],
    hdrs = [
        "StateSyncBuilder.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:utils",
        "//app/dispatcher:command_context",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "bootstrap_command",
    srcs = ["BootstrapCommand.cpp"],
    hdrs = [
        "BootstrapCommand.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        ":state_sync_builder",
        "//proto:envelope_cc_proto",
        "//proto/event:activity_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:realtime_cc_proto",
        "//proto/event:state_cc_proto",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "authenticate_command",
    srcs = ["AuthenticateCommand.cpp"],
    hdrs = [
        "AuthenticateCommand.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//proto:envelope_cc_proto",
        "//proto/command:session_cc_proto",
        "//proto/event:activity_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:session_cc_proto",
    ],
)

cc_library(
    name = "disconnection_command",
    srcs = ["DisconnectionCommand.cpp"],
    hdrs = [
        "DisconnectionCommand.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//proto/event:realtime_cc_proto",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "request_state_sync_command",
    srcs = ["RequestStateSyncCommand.cpp"],
    hdrs = [
        "RequestStateSyncCommand.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        ":state_sync_builder",
        "//app/dispatcher:command_context",
        "//proto:envelope_cc_proto",
        "//proto/command:session_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)
