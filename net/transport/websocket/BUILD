load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "text_per_socket_data",
    hdrs = ["PerSocketData.h"],
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
    defines = select({
        "//server:ssl_on": ["USE_SSL"],
        "//server:ssl_off": [],
        "//conditions:default": [],
    }),
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
    name = "text_websocket_utils",
    srcs = ["utils.cpp"],
    hdrs = ["utils.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        "//proto:envelope_cc_proto",
        "//proto/system:heartbeat_cc_proto",
    ],
)

cc_library(
    name = "text_websocket_transport",
    srcs = ["WebSocketTransport.cpp"],
    hdrs = [
        "WebSocketTransport.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//net:__subpackages__"],
    deps = [
        ":text_websocket_utils",
        ":ws_app_factory",
        "//core:server_config",
        "//infra/security:supabase_jwt_verifier",
        "//net/connection:connection_registery",
        "//net/outbound:outgoing_worker",
        "//net/runtime:heartbeat_service",
        "//net/transport:outbound_transport_iface",
        "//net/transport:i_transport",
        "//proto/command:session_cc_proto",
        "//utils:metrics",
        "@uwebsockets",
    ],
)
