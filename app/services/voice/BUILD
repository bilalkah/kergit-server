load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "voice_service",
    hdrs = [
        "VoiceService.h",
        "VoiceSessionManager.h",
    ],
    srcs = [
        "VoiceService.cpp",
        "VoiceSessionManager.cpp",
    ],
    copts = ["-std=gnu++23"],
    deps = [
        "//domains/ids:domain_ids",
        "//infra/persistence:persistence_gateway_lib",
        "//app/managers/session:session_manager",
        "//livekit/token:livekit_token_service",
        "//livekit/routing:livekit_node_registry",
        "//livekit/crypto:livekit_e2ee_key_manager",
        "//livekit/cli:livekit_client",
        "//livekit/webhook:livekit_webhook_server",
        "//proto/event:activity_cc_proto",
        "//utils:loggable",
    ],
    visibility = ["//visibility:public"],
)
