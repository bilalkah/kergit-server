load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "server",
    srcs = ["server.cpp"],
    linkopts = [
        "-lpthread",
        "-lz",
    ],
    deps = [
        "//common",
        "//core/database:chatdb_lib",
        "//server:chat_server_app_lib",
        "//server:message_filters",
        "//server:server_commands",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)
