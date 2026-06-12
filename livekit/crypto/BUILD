load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "livekit_e2ee_key_manager",
    hdrs = ["E2EEKeyManager.h"],
    srcs = ["E2EEKeyManager.cpp"],
    deps = [
        "//domains/ids:domain_ids",
        "@jwt-cpp//:jwt-cpp",
    ],
    linkopts = [
        "-lssl",
        "-lcrypto",
    ],
    visibility = ["//visibility:public"],
)
