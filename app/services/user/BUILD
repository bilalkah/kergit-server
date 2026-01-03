load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "user_cache",
    srcs = ["UserCache.cpp"],
    hdrs = ["UserCache.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//core/cache:on_memory_cache",
        "//domains:user_domain",
    ],
)

cc_library(
    name = "user_service",
    srcs = ["UserService.cpp"],
    hdrs = ["UserService.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        ":user_cache",
        "//domains:user_domain",
        "//infra/persistence:persistence_gateway_lib",
    ],
)
