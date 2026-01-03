load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

config_setting(
    name = "ssl_on",
    values = {"define": "USE_SSL=true"},
)

config_setting(
    name = "ssl_off",
    values = {"define": "USE_SSL=false"},
)

cc_binary(
    name = "fake_discord",
    srcs = [
        "Server.cpp",
        "Server.h",
        "main.cpp",
    ],
    copts = ["-std=gnu++23"],
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
        "//app:app_stack",
        "//core:server_config",
        "//net:network_router",
        "//net:network_stack",
        "//utils:env_loader",
        "//utils:loggable",
    ],
)
