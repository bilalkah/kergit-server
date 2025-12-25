load("@rules_cc//cc:cc_library.bzl", "cc_library")

SERVICE_DEPS = [
    "//app/memory:on_memory_cache",
    "//app/services:hub_publisher",
    "//app/services:public_id_service",
    "//infra/persistence:persistence_gateway_lib",
    "//net:client_gateway",
    "//net:connection_manager",
]

cc_library(
    name = "i_command",
    hdrs = [
        "ICommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/queue:outgoing_queue",
        "//domains/ids:domain_ids",
        "//net:persocketdata",
        "@nlohmann_json",
    ] + SERVICE_DEPS,
)

cc_library(
    name = "auth_command",
    srcs = ["AuthCommand.cpp"],
    hdrs = [
        "AuthCommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/queue:outgoing_queue",
        "//app/services:auth_service",
        "//domains:channel_domain",
        "//domains:hub_domain",
    ],
)

cc_library(
    name = "join_channel_command",
    srcs = ["JoinChannelCommand.cpp"],
    hdrs = ["JoinChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//domains:channel_domain",
        "//domains:message_domain",
    ],
)

cc_library(
    name = "send_message_command",
    srcs = ["SendMessageCommand.cpp"],
    hdrs = ["SendMessageCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//domains:message_domain",
    ],
)

cc_library(
    name = "create_channel_command",
    srcs = ["CreateChannelCommand.cpp"],
    hdrs = ["CreateChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//domains:hub_domain",
    ],
)

cc_library(
    name = "create_hub_command",
    srcs = ["CreateHubCommand.cpp"],
    hdrs = ["CreateHubCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//domains:channel_domain",
    ],
)

cc_library(
    name = "update_profile_command",
    srcs = ["UpdateProfileCommand.cpp"],
    hdrs = ["UpdateProfileCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)

cc_library(
    name = "delete_channel_command",
    srcs = ["DeleteChannelCommand.cpp"],
    hdrs = ["DeleteChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)

cc_library(
    name = "rename_channel_command",
    srcs = ["RenameChannelCommand.cpp"],
    hdrs = ["RenameChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)

cc_library(
    name = "rename_hub_command",
    srcs = ["RenameHubCommand.cpp"],
    hdrs = ["RenameHubCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)

cc_library(
    name = "delete_hub_command",
    srcs = ["DeleteHubCommand.cpp"],
    hdrs = ["DeleteHubCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)

cc_library(
    name = "leave_hub_command",
    srcs = ["LeaveHubCommand.cpp"],
    hdrs = ["LeaveHubCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)

cc_library(
    name = "get_hub_invite_command",
    srcs = ["GetHubInviteCommand.cpp"],
    hdrs = ["GetHubInviteCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)

cc_library(
    name = "join_hub_by_invite_command",
    srcs = ["JoinHubByInviteCommand.cpp"],
    hdrs = ["JoinHubByInviteCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)

cc_library(
    name = "update_member_role_command",
    srcs = ["UpdateMemberRoleCommand.cpp"],
    hdrs = ["UpdateMemberRoleCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
    ],
)
