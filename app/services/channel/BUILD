load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "channel_cache",
    srcs = ["ChannelCache.cpp"],
    hdrs = ["ChannelCache.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app/services:__subpackages__"],
    deps = [
        "//core/cache:on_memory_cache",
        "//domains:channel_domain",
    ],
)

cc_library(
    name = "channel_service",
    srcs = ["ChannelService.cpp"],
    hdrs = ["ChannelService.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app/services:__subpackages__"],
    deps = [
        ":channel_cache",
        "//domains:channel_domain",
        "//domains:message_domain",
        "//infra/persistence:persistence_gateway_lib",
    ],
)
