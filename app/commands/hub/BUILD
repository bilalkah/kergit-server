load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "create_hub_command",
    srcs = ["CreateHubCommand.cpp"],
    hdrs = ["CreateHubCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/commands/session:state_sync_builder",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/domain:channel_cc_proto",
        "//proto/domain:hub_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "join_hub_command",
    srcs = ["JoinHubByInviteCommand.cpp"],
    hdrs = ["JoinHubByInviteCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/commands/session:state_sync_builder",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/domain:channel_cc_proto",
        "//proto/domain:hub_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "create_hub_join_code_command",
    srcs = ["GetHubInviteCommand.cpp"],
    hdrs = ["GetHubInviteCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/services/hub:role_policy",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "validate_hub_invite_command",
    srcs = ["ValidateHubInviteCommand.cpp"],
    hdrs = ["ValidateHubInviteCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/commands:utils",
        "//app/dispatcher:command_context",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/domain:hub_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "kick_hub_member_command",
    srcs = ["KickHubMemberCommand.cpp"],
    hdrs = ["KickHubMemberCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//app/services/hub:role_policy",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "leave_hub_command",
    srcs = ["LeaveHubCommand.cpp"],
    hdrs = ["LeaveHubCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "remove_hub_command",
    srcs = ["DeleteHubCommand.cpp"],
    hdrs = ["DeleteHubCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "update_hub_member_role_command",
    srcs = ["UpdateHubMemberRoleCommand.cpp"],
    hdrs = ["UpdateHubMemberRoleCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//app/services/hub:role_policy",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/domain:hub_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)

cc_library(
    name = "update_hub_command",
    srcs = ["UpdateHubCommand.cpp"],
    hdrs = ["UpdateHubCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:hub_domain",
        "//proto:envelope_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:state_cc_proto",
    ],
)
