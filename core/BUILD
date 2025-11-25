load("@rules_cc//cc:defs.bzl", "cc_library")

config_setting(
    name = "ssl_on",
    values = {"define": "USE_SSL=true"},
)

config_setting(
    name = "ssl_off",
    values = {"define": "USE_SSL=false"},
)

cc_library(
    name = "types",
    hdrs = [
        "Types.h",
    ],
    defines = select({
        ":ssl_on": ["USE_SSL"],
        ":ssl_off": [],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = [
        "//net:persocketdata",
        "@uwebsockets",
    ],
)

cc_library(
    name = "server_config",
    hdrs = [
        "ServerConfig.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//utils:env_loader",
    ],
)

cc_library(
    name = "i_app",
    hdrs = [
        "IApp.h",
    ],
    visibility = ["//visibility:public"],
    deps = [":types"],
)

cc_library(
    name = "plain_app",
    hdrs = [
        "PlainApp.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":i_app",
        "//utils:loggable",
    ],
)

cc_library(
    name = "tls_app",
    hdrs = [
        "TlsApp.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":i_app",
        "//utils:loggable",
        "//utils:tls_config",
    ],
)

cc_library(
    name = "app_factory",
    hdrs = [
        "AppFactory.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":i_app",
        ":plain_app",
        ":tls_app",
        "//utils:tls_config",
    ],
)

cc_library(
    name = "chat_server_app",
    srcs = ["ChatServerApp.cpp"],
    hdrs = ["ChatServerApp.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":app_factory",
        ":server_config",
        ":types",
        "//app:command_registry",
        "//app:dispatcher",
        "//app/queue:event_queue",
        "//app/queue:outgoing_queue",
        "//app/services:hub_publisher",
        "//app/services:public_id_service",
        "//app/worker:worker_pool",
        "//infra/persistence:persistence_gateway_lib",
        "//net:websocket_server",
        "//utils:loggable",
    ],
)
