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
        "//app/commands:utils",
        "//domains/ids:domain_ids",
        "//infra/persistence:persistence_gateway_lib",
        "//infra/redis:redis_client",
        "//app/managers/session:session_manager",
        "//app/managers/subscription:subscription_manager",
        "//app/services/hub:hub_service",
        "//livekit/token:livekit_token_service",
        "//livekit/routing:livekit_node_registry",
        "//livekit/crypto:livekit_e2ee_key_manager",
        "//livekit/cli:livekit_client",
        "//livekit/webhook:livekit_webhook_server",
        "//net/outbound:i_outbound_sink",
        "//proto/event:activity_cc_proto",
        "//proto/event:state_cc_proto",
        "//proto:envelope_cc_proto",
        "//utils:env_loader",
        "//utils:loggable",
        "//utils:metrics",
        "@nlohmann_json",
    ],
    visibility = ["//visibility:public"],
)
