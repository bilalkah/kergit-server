load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "connection_registery",
    srcs = ["ConnectionRegistery.cpp"],
    hdrs = ["ConnectionRegistery.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
        "//net:net_types",
        "//net/transport:handle",
        "//net/transport/websocket:uws_types",
    ],
)
