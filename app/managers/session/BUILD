load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "session_manager",
    srcs = [
        "SessionManager.cpp",
    ],
    hdrs = [
        "SessionId.h",
        "SessionInfo.h",
        "SessionManager.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
        "//proto/event:error_cc_proto",
        "//utils:loggable",
        "//utils:metrics",
    ],
)

