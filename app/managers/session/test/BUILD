load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_test(
    name = "session_manager_test",
    srcs = ["SessionManager_test.cc"],
    copts = ["-std=gnu++23"],
    deps = [
        "//app/managers/session:session_manager",
        "@googletest//:gtest_main",
    ],
)
