cc_library(
    name = "server_commands",
    hdrs = [
        "commands/ICommand.h",
        "commands/JoinCommand.h",
        "commands/ChatCommand.h",
        "commands/ListCommand.h",
        "commands/PingCommand.h",
        "commands/UsersCommand.h",
        "commands/AllCommands.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = ["//common:common", "@uwebsockets//:uwebsockets"],
)

cc_library(
    name = "chat_server_app_lib",
    srcs = ["ChatServerApp.cpp"],
    hdrs = ["ChatServerApp.h"],
    deps = [
        ":server_commands",
        "//common:common",
        "@uwebsockets//:uwebsockets",
        "@nlohmann_json//:nlohmann_json",
    ],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread", "-lz"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "chat_server_lib",
    srcs = ["ChatServer.cpp"],
    hdrs = ["ChatServer.h"],
    deps = [
        ":chat_server_app_lib",
    ],
    copts = ["-std=c++17"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "server",
    srcs = ["server.cpp"],
    deps = [
        ":chat_server_app_lib",
        "@nlohmann_json//:nlohmann_json",
        "@uwebsockets//:uwebsockets",
        "//common:common",
        ":server_commands",
    ],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread", "-lz"],
)
