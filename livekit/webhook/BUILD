load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_library(
    name = "livekit_webhook_server",
    hdrs = ["LivekitWebhookServer.h","LivekitEvent.h"],
    srcs = ["LivekitWebhookServer.cpp"],
    copts = ["-std=gnu++23"],
    linkopts = ["-lcrypto", "-lz"],
    deps = [
        "@uwebsockets",
        "@nlohmann_json",
        "@jwt-cpp//:jwt-cpp",
        "//domains/ids:domain_ids",
        "//utils:loggable",
    ],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "livekit_webhook_server_test",
    srcs = ["LivekitWebhookServer_test.cc"],
    copts = ["-std=gnu++23"],
    linkopts = ["-lcrypto", "-lz"],
    deps = [
        ":livekit_webhook_server",
        "@googletest//:gtest_main",
        "@jwt-cpp//:jwt-cpp",
    ],
)
