load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "connection_registery",
    srcs = ["ConnectionRegistery.cpp"],
    hdrs = ["ConnectionRegistery.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
        "//net:net_types",
        "//net/outbound:outbound_msg",
        "//net/transport:handle",
        "//net/transport/websocket:uws_types",
        "//utils:metrics",
    ],
)
