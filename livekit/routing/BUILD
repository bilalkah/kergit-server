load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_library(
    name = "livekit_node_registry",
    srcs = ["LivekitNodeRegistry.cpp"],
    hdrs = ["LivekitNodeRegistry.h"],
    deps = [
        "//domains/ids:domain_ids",
    ],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "livekit_node_registry_test",
    srcs = ["LivekitNodeRegistry_test.cc"],
    deps = [
        ":livekit_node_registry",
        "@googletest//:gtest_main",
    ],
)
