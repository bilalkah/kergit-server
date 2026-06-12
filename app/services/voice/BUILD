load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_library(
    name = "reconcile_evidence_tracker",
    hdrs = ["ReconcileEvidenceTracker.h"],
    srcs = ["ReconcileEvidenceTracker.cpp"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = ["//domains/ids:domain_ids"],
)

cc_test(
    name = "reconcile_evidence_tracker_test",
    srcs = ["ReconcileEvidenceTrackerTest.cpp"],
    copts = ["-std=gnu++23"],
    deps = [
        ":reconcile_evidence_tracker",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "voice_session_manager",
    hdrs = ["VoiceSessionManager.h"],
    srcs = ["VoiceSessionManager.cpp"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/managers/session:session_manager",
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "voice_service",
    hdrs = [
        "VoiceService.h",
        "VoiceSessionManager.h",
    ],
    srcs = [
        "VoiceService.cpp",
    ],
    copts = ["-std=gnu++23"],
    linkopts = ["-lcrypto"],
    deps = [
        "//app/commands:utils",
        "//domains/ids:domain_ids",
        "//infra/persistence:persistence_gateway_lib",
        "//infra/redis:redis_client",
        "//app/managers/session:session_manager",
        "//app/managers/subscription:subscription_manager",
        "//app/services/hub:hub_service",
        "//core:livekit_cluster_config",
        ":reconcile_evidence_tracker",
        ":voice_session_manager",
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
