load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "auth_service",
    srcs = ["AuthService.cpp"],
    hdrs = [
        "AuthService.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//infra/security:supabase_jwt_verifier",
        "//utils:loggable",
    ],
)
