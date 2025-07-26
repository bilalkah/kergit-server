load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "server_commands",
    hdrs = [
        "commands/AllCommands.h",
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
        "//common",
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
        ":server_commands",
        "//common",
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
        ":server_commands",
        "//common",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)
