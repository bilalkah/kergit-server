load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "pubsub",
    srcs = [
        "PubSub.cpp",
    ],
    hdrs = [
        "PubSub.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)