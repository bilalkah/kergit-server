load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "session_manager",
    srcs = [
        "SessionManager.cpp",
    ],
    hdrs = [
        "SessionInfo.h",
        "SessionManager.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
        "//utils:metrics",
    ],
)
