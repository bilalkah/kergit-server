load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")
load("@rules_python//python:defs.bzl", "py_binary")

py_binary(
    name = "auth_test_client",
    srcs = ["auth_test_client.py"],
    env = {
        "PYTHONPATH": "/home/bilal/.local/lib/python3.10/site-packages",
    },
    main = "auth_test_client.py",
    python_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [],
)

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
        "//clients/cli:chat_client_lib",
        "//server:chat_server_lib",
        "@googletest//:gtest_main",
        "@nlohmann_json",
    ],
)
