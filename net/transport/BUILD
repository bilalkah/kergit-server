load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "handle",
    hdrs = ["Handle.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//net/transport/websocket:uws_types",
    ],
)

cc_library(
    name = "i_loop",
    hdrs = ["ILoop.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "@uwebsockets",
    ],
)

cc_library(
    name = "i_transport",
    hdrs = ["ITransport.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
)
