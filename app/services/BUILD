load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "auth_service",
    srcs = ["AuthService.cpp"],
    hdrs = [
        "AuthService.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "//infra/security:supabase_jwt_verifier",
        "//net:persocketdata",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "hub_publisher",
    srcs = ["HubPublisher.cpp"],
    hdrs = ["HubPublisher.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//core:i_app",
        "//infra/persistence:chatdb_lib",
        "//net:client_gateway",
        "//net:connection_manager",
        "@nlohmann_json",
        "@uwebsockets",
    ],
)
