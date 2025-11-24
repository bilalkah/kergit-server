load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//app/commands:command_list.bzl", "COMMAND_LIST")

config_setting(
    name = "ssl_on",
    values = {"define": "USE_SSL=true"},
)

config_setting(
    name = "ssl_off",
    values = {"define": "USE_SSL=false"},
)

cc_library(
    name = "dispatcher",
    srcs = ["Dispatcher.cpp"],
    hdrs = ["Dispatcher.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/commands:i_command",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "command_registry",
    srcs = ["CommandRegistery.cpp"],
    hdrs = [
        "CommandRegistery.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":dispatcher",
    ] + COMMAND_LIST,
)

cc_binary(
    name = "server_app",
    srcs = ["server.cpp"],
    data = select({
        ":ssl_on": [
            "//certs:cert.pem",
            "//certs:key.pem",
        ],
        ":ssl_off": [],
        "//conditions:default": [],
    }),
    defines = select({
        ":ssl_on": ["USE_SSL"],
        ":ssl_off": [],
        "//conditions:default": [],
    }),
    linkopts = [
        "-pthread",
        "-lz",
    ] + select({
        ":ssl_on": [
            "-lssl",
            "-lcrypto",
        ],
        ":ssl_off": [],
        # default condition is ssl_off
        "//conditions:default": [],
    }),
    deps = [
        "//core:chat_server_app",
        "//core:server_config",
        "//utils:env_loader",
        "//utils:loggable",
        "@redis_plus_plus",
    ],
)

