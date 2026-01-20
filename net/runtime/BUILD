load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "heartbeat_service",
    srcs = ["HeartbeatService.cpp"],
    hdrs = ["HeartbeatService.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//net/connection:connection_registery",
        "//net/transport:i_loop",
        "@fmt",
        "@uwebsockets",
    ],
)
