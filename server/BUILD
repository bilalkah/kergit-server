cc_library(
    name = "server_commands",
    hdrs = [
        "commands/ICommand.h",
        "commands/JoinCommand.h",
        "commands/ChatCommand.h",
        "commands/ListCommand.h",
        "commands/PingCommand.h",
        "commands/AllCommands.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = ["//common:common", "@uwebsockets//:uwebsockets"],
)

cc_binary(
    name = "server",
    srcs = ["server.cpp"],
    deps = [
        "@nlohmann_json//:nlohmann_json",
        "@uwebsockets//:uwebsockets",
        "//common:common",
        ":server_commands",
    ],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread", "-lz"],
)
