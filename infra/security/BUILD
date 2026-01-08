load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "supabase_jwt_verifier",
    srcs = ["token/SupabaseJwtVerifier.cpp"],
    hdrs = [
        "token/ITokenVerifier.h",
        "token/SupabaseJwtVerifier.h",
    ],
    copts = ["-std=gnu++23"],
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
    srcs = ["validation/JsonValidator.cpp"],
    hdrs = ["validation/JsonValidator.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "@nlohmann_json",
    ],
)

cc_library(
    name = "proto_message_validator",
    srcs = ["validation/ProtoValidator.cpp"],
    hdrs = ["validation/ProtoValidator.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        "//proto:chat_cc_proto",
        "//proto:envelope_cc_proto",
    ],
)
