load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")

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

cc_binary(
    name = "voice_call_test",
    srcs = ["voice_call_test.cpp"],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread"],
    deps = [
        "//client:webrtc_manager_lib",
    ],
)
