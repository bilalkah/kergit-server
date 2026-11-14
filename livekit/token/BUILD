load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "livekit_token_service",
    srcs = ["LiveKitTokenService.cpp"],
    hdrs = ["LiveKitTokenService.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "@jwt-cpp//:jwt-cpp",
    ],
)
