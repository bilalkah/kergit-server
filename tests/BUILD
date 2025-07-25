cc_test(
    name = "chat_integration_test",
    srcs = [
        "ChatIntegrationTests.cpp",
        "ChatTestFixture.h",
    ],
    deps = [
        "//server:chat_server_lib",
        "//client:chat_client_lib",
        "@googletest//:gtest_main",
        "@nlohmann_json//:nlohmann_json",
    ],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread"],
    timeout = "long",
) 