load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "i_command",
    hdrs = [
        "ICommand.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "//net:persocketdata",
        "@nlohmann_json",
    ],
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
        "//app/services:auth_service",
        "//app/services:hub_publisher",
        "//infra/persistence:chatdb_lib",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//net:client_gateway",
        "//net:connection_manager",
    ],
)

cc_library(
    name = "list_command",
    srcs = ["ListCommand.cpp"],
    hdrs = ["ListCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//app/services:hub_publisher",
        "//infra/persistence:chatdb_lib",
        "//net:connection_manager",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "join_channel_command",
    srcs = ["JoinChannelCommand.cpp"],
    hdrs = ["JoinChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//infra/persistence:chatdb_lib",
        "//net:client_gateway",
        "//net:connection_manager",
        "//domains:channel_domain",
        "//domains:message_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "channel_users_command",
    srcs = ["ChannelUsersCommand.cpp"],
    hdrs = ["ChannelUsersCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//net:connection_manager",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "send_message_command",
    srcs = ["SendMessageCommand.cpp"],
    hdrs = ["SendMessageCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//infra/persistence:chatdb_lib",
        "//net:client_gateway",
        "//domains:message_domain",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "create_channel_command",
    srcs = ["CreateChannelCommand.cpp"],
    hdrs = ["CreateChannelCommand.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_command",
        "//infra/persistence:chatdb_lib",
        "//app/services:hub_publisher",
        "//domains:hub_domain",
        "@nlohmann_json",
    ],
)
