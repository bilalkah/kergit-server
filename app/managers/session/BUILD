load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "session_manager",
    srcs = [
        "SessionManager.cpp",
    ],
    hdrs = [
        "SessionManager.h",
        "SessionInfo.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)
