load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "livekit_e2ee_key_manager",
    srcs = ["E2EEKeyManager.cpp"],
    hdrs = ["E2EEKeyManager.h"],
    linkopts = [
        "-lssl",
        "-lcrypto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "@jwt-cpp//:jwt-cpp",
    ],
)
