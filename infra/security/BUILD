load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "supabase_jwt_verifier",
    srcs = [
        "token/JwtJsonUtils.cpp",
        "token/SupabaseVerifier.cpp",
    ],
    hdrs = [
        "token/ITokenVerifier.h",
        "token/JwtJsonUtils.h",
        "token/JwtTypes.h",
        "token/SupabaseVerifier.h",
    ],
    copts = ["-std=gnu++23"],
    linkopts = ["-lcrypto"],  # OpenSSL crypto library
    visibility = ["//visibility:public"],
    deps = [
        "//utils:env_loader",
        "//utils:loggable",
        "@jwt-cpp//:jwt-cpp",
        "@nlohmann_json",
    ],
)
