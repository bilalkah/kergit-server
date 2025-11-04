load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "supabase_jwt_verifier",
    srcs = ["token/SupabaseJwtVerifier.cpp"],
    hdrs = [
        "token/ITokenVerifier.h",
        "token/SupabaseJwtVerifier.h",
    ],
    linkopts = ["-lcrypto"],  # OpenSSL crypto library
    visibility = ["//visibility:public"],
    deps = [
        "//utils:env_loader",
        "//utils:loggable",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "message_validator",
    srcs = ["validation/MessageValidator.cpp"],
    hdrs = ["validation/MessageValidator.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//app:dispatcher",
        "@nlohmann_json",
    ],
)
