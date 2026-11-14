load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "livekit_client",
    srcs = ["LivekitClient.cpp"],
    hdrs = ["LivekitClient.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "//livekit/token:livekit_token_service",
        "//utils:loggable",
        "@cpr",
        "@nlohmann_json",
    ],
)
