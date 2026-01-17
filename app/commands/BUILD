load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "i_command",
    hdrs = [
        "CommandJson.h",
        "ICommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "//proto/event:error_cc_proto",
    ],
)

cc_library(
    name = "join_channel_command",
    srcs = ["channel/JoinChannelCommand.cpp"],
    hdrs = ["channel/JoinChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//domains:message_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "send_message_command",
    srcs = ["message/SendMessageCommand.cpp"],
    hdrs = ["message/SendMessageCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/session:session_manager",
        "//app/managers/subscription:subscription_manager",
        "//app/managers/subscription:topic",
        "//app/services/channel:channel_service",
        "//app/services/hub:hub_service",
        "//app/services/user:user_service",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//domains:message_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "create_channel_command",
    srcs = ["channel/CreateChannelCommand.cpp"],
    hdrs = ["channel/CreateChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/session:session_manager",
        "//app/managers/subscription:subscription_manager",
        "//app/managers/subscription:topic",
        "//app/services/channel:channel_service",
        "//app/services/hub:hub_service",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "delete_channel_command",
    srcs = ["channel/DeleteChannelCommand.cpp"],
    hdrs = ["channel/DeleteChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/session:session_manager",
        "//app/managers/subscription:subscription_manager",
        "//app/managers/subscription:topic",
        "//app/services/channel:channel_service",
        "//app/services/hub:hub_service",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "rename_channel_command",
    srcs = ["channel/RenameChannelCommand.cpp"],
    hdrs = ["channel/RenameChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "delete_hub_command",
    srcs = ["hub/DeleteHubCommand.cpp"],
    hdrs = ["hub/DeleteHubCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "create_hub_command",
    srcs = ["hub/CreateHubCommand.cpp"],
    hdrs = ["hub/CreateHubCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "rename_hub_command",
    srcs = ["hub/RenameHubCommand.cpp"],
    hdrs = ["hub/RenameHubCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "get_hub_invite_command",
    srcs = ["hub/GetHubInviteCommand.cpp"],
    hdrs = ["hub/GetHubInviteCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "join_hub_by_invite_command",
    srcs = ["hub/JoinHubByInviteCommand.cpp"],
    hdrs = ["hub/JoinHubByInviteCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "leave_hub_command",
    srcs = ["hub/LeaveHubCommand.cpp"],
    hdrs = ["hub/LeaveHubCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "update_member_role_command",
    srcs = ["member/UpdateMemberRoleCommand.cpp"],
    hdrs = ["member/UpdateMemberRoleCommand.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//app/managers/subscription:topic",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "update_profile_command",
    srcs = ["profile/UpdateProfileCommand.cpp"],
    hdrs = ["profile/UpdateProfileCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/dispatcher:command_context",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)
# cc_library(
#     name = "auth_command",
#     srcs = ["AuthCommand.cpp"],
#     hdrs = [
#         "AuthCommand.h",
#     ],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#         "//app/queue:outgoing_queue",
#         "//app/services:auth_service",
#         "//domains:channel_domain",
#         "//domains:hub_domain",
#     ],
# )

# cc_library(
#     name = "join_channel_command",
#     srcs = ["JoinChannelCommand.cpp"],
#     hdrs = ["JoinChannelCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#         "//domains:channel_domain",
#         "//domains:message_domain",
#     ],
# )

# cc_library(
#     name = "send_message_command",
#     srcs = ["SendMessageCommand.cpp"],
#     hdrs = ["SendMessageCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#         "//domains:message_domain",
#     ],
# )

# cc_library(
#     name = "create_channel_command",
#     srcs = ["CreateChannelCommand.cpp"],
#     hdrs = ["CreateChannelCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#         "//domains:hub_domain",
#     ],
# )

# cc_library(
#     name = "create_hub_command",
#     srcs = ["CreateHubCommand.cpp"],
#     hdrs = ["CreateHubCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#         "//domains:channel_domain",
#     ],
# )

# cc_library(
#     name = "update_profile_command",
#     srcs = ["UpdateProfileCommand.cpp"],
#     hdrs = ["UpdateProfileCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#     ],
# )

# cc_library(
#     name = "delete_channel_command",
#     srcs = ["DeleteChannelCommand.cpp"],
#     hdrs = ["DeleteChannelCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#     ],
# )

# cc_library(
#     name = "rename_channel_command",
#     srcs = ["RenameChannelCommand.cpp"],
#     hdrs = ["RenameChannelCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#     ],
# )

# cc_library(
#     name = "rename_hub_command",
#     srcs = ["RenameHubCommand.cpp"],
#     hdrs = ["RenameHubCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#     ],
# )

# cc_library(
#     name = "delete_hub_command",
#     srcs = ["DeleteHubCommand.cpp"],
#     hdrs = ["DeleteHubCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#     ],
# )

# cc_library(
#     name = "leave_hub_command",
#     srcs = ["LeaveHubCommand.cpp"],
#     hdrs = ["LeaveHubCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#     ],
# )

# cc_library(
#     name = "get_hub_invite_command",
#     srcs = ["GetHubInviteCommand.cpp"],
#     hdrs = ["GetHubInviteCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#     ],
# )

# cc_library(
#     name = "join_hub_by_invite_command",
#     srcs = ["JoinHubByInviteCommand.cpp"],
#     hdrs = ["JoinHubByInviteCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#     ],
# )

# cc_library(
#     name = "update_member_role_command",
#     srcs = ["UpdateMemberRoleCommand.cpp"],
#     hdrs = ["UpdateMemberRoleCommand.h"],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":i_command",
#     ],
# )
