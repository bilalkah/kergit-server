load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "topic",
    hdrs = ["Topic.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "subscription_manager",
    srcs = [
        "SubscriptionManager.cpp",
    ],
    hdrs = [
        "ISubscriptionManager.h",
        "SubscriptionManager.h",
        "Topic.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        ":topic",
        "@fmt",
    ],
)
