load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "hub_cache",
    srcs = ["HubCache.cpp"],
    hdrs = [
        "HubCache.h",
        "HubSnapshot.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//core/cache:on_memory_cache",
        "//domains:hub_domain",
    ],
)

cc_library(
    name = "hub_service",
    srcs = ["HubService.cpp"],
    hdrs = [
        "HubService.h",
        "HubSnapshot.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        ":hub_cache",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//infra/persistence:persistence_gateway_lib",
        "//utils:loggable",
    ],
)
