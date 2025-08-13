load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "libpqxx_integration_test",
    srcs = ["libpqxx_integration_test.cpp"],
    deps = [
        "@googletest//:gtest_main",
        "@libpqxx",
    ],
)
