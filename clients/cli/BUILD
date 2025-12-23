load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "chat_client_lib",
    srcs = ["ChatClient.cpp"],
    hdrs = ["ChatClient.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":chat_client_app_lib",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "chat_client_app_lib",
    srcs = ["ChatClientApp.cpp"],
    hdrs = [
        "ChatClient.h",
        "ChatClientApp.h",
    ],
    copts = ["-std=gnu++23"],
    includes = ["."],
    linkopts = ["-lpthread"],
    strip_include_prefix = "",
    visibility = ["//visibility:public"],
    deps = [
        "@nlohmann_json",
        "@websocketpp",
    ],
)

cc_binary(
    name = "client",
    srcs = ["client.cpp"],
    copts = ["-std=gnu++23"],
    linkopts = ["-lpthread"],
    deps = [
        ":chat_client_app_lib",
        "@nlohmann_json",
        "@websocketpp",
    ],
)
