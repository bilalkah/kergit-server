load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "chat_integration_test",
    timeout = "long",
    srcs = [
        "ChatIntegrationTests.cpp",
        "ChatTestFixture.h",
    ],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread"],
    deps = [
        "//client:chat_client_lib",
        "//server:chat_server_lib",
        "@googletest//:gtest_main",
        "@nlohmann_json",
    ],
)
