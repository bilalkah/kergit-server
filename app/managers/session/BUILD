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

cc_test(
    name = "session_manager_test",
    srcs = ["SessionManager_test.cc"],
    copts = ["-std=gnu++23"],
    deps = [
        ":session_manager",
        "@googletest//:gtest_main",
    ],
)
