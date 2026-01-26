load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "livekit_token_service",
    srcs = [
        "LiveKitTokenService.cpp",
    ],
    hdrs = [
        "LiveKitTokenService.h",
    ],
    linkopts = [
        "-lssl",
        "-lcrypto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",  # because you include domains/ids/Ids.h
        "//utils:loggable",
        "@jwt-cpp//:jwt-cpp",
    ],
)
