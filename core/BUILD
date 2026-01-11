load("@rules_cc//cc:defs.bzl", "cc_library")

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
