load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "jwt_verifier_test",
    srcs = ["jwt_verifier_test.cc"],
    data = [
        "test_token.sample",
    ],
    copts = ["-std=gnu++23"],
    deps = [
        "//infra/security:supabase_jwt_verifier",
        "//utils:env_loader",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "jwt_verifier_benchmark_test",
    srcs = ["jwt_verifier_benchmark_test.cc"],
    data = [
        "test_token.sample",
    ],
    copts = ["-std=gnu++23"],
    deps = [
        "//infra/security:supabase_jwt_verifier",
        "//utils:env_loader",
        "@googletest//:gtest_main",
    ],
)
