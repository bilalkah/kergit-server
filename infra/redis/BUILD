load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "redis_client",
    srcs = ["RedisClient.cpp"],
    hdrs = ["RedisClient.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/hiredis",
        "//third_party/redis_plus_plus",
    ],
)
