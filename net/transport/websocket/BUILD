load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "text_per_socket_data",
    hdrs = ["TextPerSocketData.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "uws_types",
    hdrs = ["UwsTypes.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        ":text_per_socket_data",
        "@uwebsockets",
    ],
)

cc_library(
    name = "iws_app",
    hdrs = [
        "IWsApp.h",
    ],
    visibility = ["//net:__subpackages__"],
    deps = [":uws_types"],
)

cc_library(
    name = "ws_app_factory",
    hdrs = [
        "WsAppFactory.h",
    ],
    visibility = ["//net:__subpackages__"],
    deps = [
        ":iws_app",
        "//utils:tls_config",
    ],
)

cc_library(
    name = "text_websocket_transport",
    srcs = ["TextWebSocketTransport.cpp"],
    hdrs = ["TextWebSocketTransport.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        ":ws_app_factory",
        "//core:server_config",
        "//infra/security:supabase_jwt_verifier",
        "//net/outbound:outgoing_worker",
        "//net/runtime:heartbeat_service",
        "//net/transport:i_transport",
        "//proto:envelope_cc_proto",
        "//proto:heartbeat_cc_proto",
        "@uwebsockets",
    ],
)
