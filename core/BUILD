load("@rules_cc//cc:defs.bzl", "cc_library")

config_setting(
    name = "ssl_on",
    values = {"define": "USE_SSL=true"},
)

config_setting(
    name = "ssl_off",
    values = {"define": "USE_SSL=false"},
)

cc_library(
    name = "types",
    hdrs = [
        "Types.h",
    ],
    defines = select({
        ":ssl_on": ["USE_SSL"],
        ":ssl_off": [],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = [
        "//net:persocketdata",
        "@uwebsockets",
    ],
)

cc_library(
    name = "server_config",
    hdrs = [
        "ServerConfig.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//utils:env_loader",
        "//utils:tls_config",
    ],
)
