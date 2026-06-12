load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_library(
    name = "ws_origin_policy",
    srcs = ["WsOriginPolicy.cpp"],
    hdrs = ["WsOriginPolicy.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//utils:path_resolver",
    ],
)

cc_test(
    name = "ws_origin_policy_test",
    srcs = ["WsOriginPolicy_test.cc"],
    data = ["//config:ws_origin_policy.yaml"],
    copts = ["-std=gnu++23"],
    deps = [
        ":ws_origin_policy",
        "@googletest//:gtest_main",
    ],
)
