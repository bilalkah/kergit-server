load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "livekit_token_service",
    hdrs = ["LiveKitTokenService.h"],
    srcs = ["LiveKitTokenService.cpp"],
    deps = [
        "@jwt-cpp//:jwt-cpp",
        "//domains/ids:domain_ids",
    ],
    visibility = ["//visibility:public"],
)