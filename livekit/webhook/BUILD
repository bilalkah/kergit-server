load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_library(
    name = "livekit_webhook_server",
    srcs = ["LivekitWebhookServer.cpp"],
    hdrs = [
        "LivekitEvent.h",
        "LivekitWebhookServer.h",
    ],
    copts = ["-std=gnu++23"],
    linkopts = [
        "-lcrypto",
        "-lz",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "//utils:loggable",
        "@jwt-cpp//:jwt-cpp",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)

cc_test(
    name = "livekit_webhook_server_test",
    srcs = ["LivekitWebhookServer_test.cc"],
    copts = ["-std=gnu++23"],
    linkopts = [
        "-lcrypto",
        "-lz",
    ],
    deps = [
        ":livekit_webhook_server",
        "@googletest//:gtest_main",
        "@jwt-cpp//:jwt-cpp",
    ],
)
