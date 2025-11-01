load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "jwt_verifier_test",
    srcs = ["jwt_verifier_test.cc"],
    deps = [
        "//infra/security:supabase_jwt_verifier",
        "//utils:env_loader",
        "@googletest//:gtest_main",
    ],
)
