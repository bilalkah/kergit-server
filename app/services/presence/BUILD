load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "presence_service",
    srcs = [
        "PresenceService.cpp",
    ],
    hdrs = [
        "PresenceService.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//app/managers/session:session_manager",
        "//app/managers/subscription:subscription_manager",
        "//domains/ids:domain_ids",
    ],
)
