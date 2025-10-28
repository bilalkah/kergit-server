load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

config_setting(
    name = "ssl_on",
    values = { "define": "USE_SSL=true" },
)

config_setting(
    name = "ssl_off",
    values = { "define": "USE_SSL=false" },
)

cc_library(
    name = "server_commands",
    srcs = ["commands/AuthenticateCommand.cpp"],
    hdrs = [
        "Config.h",
        "commands/AllCommands.h",
        "commands/AuthenticateCommand.h",
        "commands/ChatCommand.h",
        "commands/ICommand.h",
        "commands/JoinCommand.h",
        "commands/ListCommand.h",
        "commands/PingCommand.h",
        "commands/UsersCommand.h",
    ],
    defines = select({
        ":ssl_on": ["USE_SSL"],
        ":ssl_off": [],
        "//conditions:default": [],
    }),
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":message_filters",
        "//common",
        "//core/database:chatdb_lib",
        "//core/security:supabase_jwt_verifier_lib",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)

cc_library(
    name = "message_filters",
    srcs = ["MessageFilters.cpp"],
    hdrs = [
        "Config.h",
        "MessageFilters.h",
    ],
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
    hdrs = [
        "ChatServerApp.h",
        "OriginAllowlist.h",
        "TlsConfig.h",
    ],
    copts = [
        "-std=c++17",
    ] + select({
        ":ssl_on": ["-DUSE_SSL"],
        ":ssl_off": [],
        "//conditions:default": [],
    }),
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
        "-lpthread",
        "-lz",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":message_filters",
        ":server_commands",
        "//common",
        "//core/database:chatdb_lib",
        "//core/security:hmac_validator_lib",
        "//core/security:message_validator_lib",
        "//core/security:rate_limiter_lib",
        "//utils:env_loader",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)
