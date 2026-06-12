load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "logger_test",
    srcs = ["logger_test.cc"],
    copts = ["-std=gnu++23"],
    deps = [
        "//utils:loggable",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "loggable_test",
    srcs = ["loggable_test.cc"],
    copts = ["-std=gnu++23"],
    deps = [
        "//utils:loggable",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "env_loader_test",
    srcs = ["env_loader_test.cc"],
    copts = ["-std=gnu++23"],
    data = [
        "sample.env",
    ],
    deps = [
        "//utils:env_loader",
        "@googletest//:gtest_main",
    ],
)
