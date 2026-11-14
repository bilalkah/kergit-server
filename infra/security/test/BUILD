load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "jwt_verifier_test",
    srcs = ["jwt_verifier_test.cc"],
    copts = ["-std=gnu++23"],
    data = [
        "test_token.sample",
    ],
    deps = [
        "//infra/security:supabase_jwt_verifier",
        "//utils:env_loader",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "jwt_verifier_benchmark_test",
    srcs = ["jwt_verifier_benchmark_test.cc"],
    copts = ["-std=gnu++23"],
    data = [
        "test_token.sample",
    ],
    deps = [
        "//infra/security:supabase_jwt_verifier",
        "//utils:env_loader",
        "@googletest//:gtest_main",
    ],
)
