load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "env_loader",
    srcs = ["EnvLoader.cpp"],
    hdrs = [
        "EnvLoader.h",
    ],
    copts = ["-std=gnu++23"],
    data = [
        "//:.env",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":loggable",
    ],
)

cc_library(
    name = "tls_config",
    hdrs = ["TlsConfig.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":loggable",
    ],
)

cc_library(
    name = "loggable",
    hdrs = [
        "Loggable.h",
        "Logger.h",
    ],
    visibility = ["//visibility:public"],
)
