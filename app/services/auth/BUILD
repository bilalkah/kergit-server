load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "auth_service",
    srcs = ["AuthService.cpp"],
    hdrs = [
        "AuthService.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//infra/security:supabase_jwt_verifier",
        "//utils:loggable",
    ],
)
