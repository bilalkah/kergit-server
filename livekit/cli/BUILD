load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "livekit_client",
    hdrs = ["LivekitClient.h"],
    srcs = ["LivekitClient.cpp"],
    deps = [
        "@cpr",
        "@nlohmann_json",
        "//domains/ids:domain_ids",
        "//livekit/token:livekit_token_service",
        "//utils:loggable",
    ],
    visibility = ["//visibility:public"],
)