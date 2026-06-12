load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "control_http_server",
    srcs = ["HttpServer.cpp"],
    hdrs = ["HttpServer.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//server:__pkg__"],
    deps = [
        "//core:server_config",
        "//utils:loggable",
        "//utils:metrics",
        "@uwebsockets",
    ],
)
