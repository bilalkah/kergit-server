load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "connection_ssl_test",
    srcs = ["connection_ssl_test.cc"],
    copts = ["-std=gnu++23"],
    deps = [
        "//core:server_config",
        "//infra/persistence:persistence_gateway_lib",
        "//utils:env_loader",
        "@googletest//:gtest_main",
        "@libpqxx",
    ],
)
