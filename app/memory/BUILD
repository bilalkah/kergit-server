load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "on_memory_cache",
    srcs = [
        "OnMemoryCache.cpp",
    ],
    hdrs = [
        "ICache.h",
        "OnMemoryCache.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//domains",
    ],
)
