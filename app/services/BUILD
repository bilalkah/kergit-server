load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "public_id_service",
    srcs = ["PublicIdService.cpp"],
    hdrs = ["PublicIdService.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

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
        ":public_id_service",
        "//app/queue:outgoing_queue",
        "//core:i_app",
        "//infra/persistence:persistence_gateway_lib",
        "//net:connection_manager",
        "@nlohmann_json",
        "@uwebsockets",
        "//utils:loggable",
    ],
)
