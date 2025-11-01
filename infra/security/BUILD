load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "supabase_jwt_verifier",
    srcs = ["token/SupabaseJWTVerifier.cpp"],
    hdrs = [
        "token/ITokenVerifier.h",
        "token/SupabaseJWTVerifier.h",
    ],
    linkopts = ["-lcrypto"],  # OpenSSL crypto library
    visibility = ["//visibility:public"],
    deps = [
        "//utils:env_loader",
        "//utils:loggable",
        "@nlohmann_json",
    ],
)
