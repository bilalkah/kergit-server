load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_library(
    name = "public_endpoint_config",
    srcs = ["PublicEndpointConfig.cpp"],
    hdrs = ["PublicEndpointConfig.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = ["//utils:env_loader"],
)

cc_test(
    name = "public_endpoint_config_test",
    srcs = ["PublicEndpointConfig_test.cc"],
    copts = ["-std=gnu++23"],
    deps = [
        ":public_endpoint_config",
        "//utils:env_loader",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "livekit_cluster_config",
    srcs = ["LivekitClusterConfig.cpp"],
    hdrs = ["LivekitClusterConfig.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":public_endpoint_config",
        "//utils:env_loader",
        "@nlohmann_json",
    ],
)

cc_test(
    name = "livekit_cluster_config_test",
    srcs = ["LivekitClusterConfig_test.cc"],
    copts = ["-std=gnu++23"],
    deps = [
        ":livekit_cluster_config",
        ":public_endpoint_config",
        "//utils:env_loader",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "server_config",
    hdrs = [
        "ServerConfig.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":livekit_cluster_config",
        ":public_endpoint_config",
        "//utils:env_loader",
        "//utils:path_resolver",
        "//utils:tls_config",
    ],
)
