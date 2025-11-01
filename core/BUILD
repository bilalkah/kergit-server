load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "types",
    hdrs = [
        "Types.h",
    ],
    visibility = ["//visibility:public"],
    deps = ["@uwebsockets"],
)

cc_library(
    name = "i_app",
    hdrs = [
        "IApp.h",
    ],
    visibility = ["//visibility:public"],
    deps = [":types"],
)

cc_library(
    name = "i_websocket",
    hdrs = ["IWebSocket.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":i_app",
        "//net:persocketdata",
    ],
)

cc_library(
    name = "plain_app",
    hdrs = [
        "PlainApp.h",
    ],
    visibility = ["//visibility:public"],
    deps = [":i_app"],
)

cc_library(
    name = "tls_app",
    hdrs = [
        "TlsApp.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":i_app",
        "//utils:tls_config",
    ],
)
