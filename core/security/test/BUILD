load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "authentication_test",
    size = "medium",
    srcs = ["authentication_test.cpp"],
    copts = ["-std=c++17"],
    deps = [
        "//core/security:authentication_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "hmac_validator_test", 
    size = "medium",
    srcs = ["hmac_validator_test.cpp"],
    copts = ["-std=c++17"],
    deps = [
        "//core/security:hmac_validator_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "jwt_manager_test",
    size = "medium", 
    srcs = ["jwt_manager_test.cpp"],
    copts = ["-std=c++17"],
    deps = [
        "//core/security:jwt_manager_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "rate_limiter_test",
    size = "medium",
    srcs = ["rate_limiter_test.cpp"], 
    copts = ["-std=c++17"],
    deps = [
        "//core/security:rate_limiter_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "message_validator_test",
    size = "medium",
    srcs = ["message_validator_test.cpp"],
    copts = ["-std=c++17"], 
    deps = [
        "//core/security:message_validator_lib",
        "@googletest//:gtest_main",
        "@nlohmann_json",
    ],
)


