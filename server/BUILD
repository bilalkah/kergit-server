load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "server_commands",
    srcs = ["commands/AuthenticateCommand.cpp"],
    hdrs = [
        "commands/AllCommands.h",
        "commands/AuthenticateCommand.h",
        "commands/ChatCommand.h",
        "commands/ICommand.h",
        "commands/JoinCommand.h",
        "commands/ListCommand.h",
        "commands/PingCommand.h",
        "commands/UsersCommand.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":message_filters",
        "//common",
        "//core/database:chatdb_lib",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)

cc_library(
    name = "message_filters",
    srcs = ["MessageFilters.cpp"],
    hdrs = ["MessageFilters.h"],
    copts = ["-std=c++17"],
    visibility = ["//visibility:public"],
    deps = [
        "@nlohmann_json",
        "@uwebsockets",
    ],
)

cc_library(
    name = "chat_server_app_lib",
    srcs = ["ChatServerApp.cpp"],
    hdrs = ["ChatServerApp.h"],
    copts = ["-std=c++17"],
    linkopts = [
        "-lpthread",
        "-lz",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":message_filters",
        ":server_commands",
        "//common",
        "//core/database:chatdb_lib",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)

cc_library(
    name = "chat_server_lib",
    srcs = ["ChatServer.cpp"],
    hdrs = ["ChatServer.h"],
    copts = ["-std=c++17"],
    visibility = ["//visibility:public"],
    deps = [
        ":chat_server_app_lib",
    ],
)

cc_binary(
    name = "server",
    srcs = ["server.cpp"],
    copts = ["-std=c++17"],
    linkopts = [
        "-lpthread",
        "-lz",
    ],
    deps = [
        ":chat_server_app_lib",
        ":message_filters",
        ":server_commands",
        "//common",
        "//core/database:chatdb_lib",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)
