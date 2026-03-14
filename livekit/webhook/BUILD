load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "livekit_webhook_server",
    hdrs = ["LivekitWebhookServer.h","LivekitEvent.h"],
    srcs = ["LivekitWebhookServer.cpp"],
    copts = ["-std=gnu++23"],
    linkopts = ["-lcrypto"],
    deps = [
        "@uwebsockets",
        "@nlohmann_json",
        "@jwt-cpp//:jwt-cpp",
        "//domains/ids:domain_ids",
        "//utils:loggable",
    ],
    visibility = ["//visibility:public"],
)
