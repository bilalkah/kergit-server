load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "authentication_lib",
    srcs = ["authentication/Authentication.cpp"],
    hdrs = ["authentication/Authentication.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//common",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "rate_limiter_lib",
    srcs = ["rate_limiter/RateLimiter.cpp"],
    hdrs = ["rate_limiter/RateLimiter.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "hmac_validator_lib",
    srcs = ["hmac_validator/HMACValidator.cpp"],
    hdrs = ["hmac_validator/HMACValidator.h"],
    linkopts = ["-lcrypto"],  # OpenSSL crypto library
    visibility = ["//visibility:public"],
)

cc_library(
    name = "supabase_jwt_verifier_lib",
    srcs = ["supabase_jwt_verifier/SupabaseJWTVerifier.cpp"],
    hdrs = ["supabase_jwt_verifier/SupabaseJWTVerifier.h"],
    linkopts = ["-lcrypto"],  # OpenSSL crypto library
    visibility = ["//visibility:public"],
    deps = ["@nlohmann_json"],
)

cc_library(
    name = "message_validator_lib",
    srcs = ["message_validator/MessageValidator.cpp"],
    hdrs = ["message_validator/MessageValidator.h"],
    visibility = ["//visibility:public"],
    deps = ["@nlohmann_json"],
)

# Legacy target for backward compatibility
cc_library(
    name = "security_basic",
    srcs = [
        "authentication/Authentication.cpp",
    ],
    hdrs = [
        "authentication/Authentication.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//common",
        "@nlohmann_json",
    ],
)
