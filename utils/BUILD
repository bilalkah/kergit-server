load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "env_loader",
    srcs = ["EnvLoader.cpp"],
    hdrs = [
        "EnvLoader.h",
    ],
    data = [
        "//:.env",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":loggable",
    ],
)

cc_library(
    name = "tls_config",
    hdrs = ["TlsConfig.h"],
    defines = select({
        "//server:ssl_on": ["USE_SSL"],
        "//server:ssl_off": [],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = [
        ":loggable",
    ],
)

cc_library(
    name = "loggable",
    hdrs = [
        "EventLogger.h",
        "Loggable.h",
        "Logger.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//core/base:ts_queue",
        "@fmt",
    ],
)

cc_library(
    name = "metrics",
    srcs = ["Metrics.cpp"],
    hdrs = ["Metrics.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":env_loader",
        ":loggable",
        "//proto:envelope_cc_proto",
        "@fmt",
    ],
)

cc_library(
    name = "path_resolver",
    hdrs = ["PathResolver.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "uuid",
    srcs = ["Uuid.cpp"],
    hdrs = ["Uuid.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
)
