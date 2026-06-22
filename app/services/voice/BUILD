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
        ":voice_session_manager",
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
    name = "voice_nonce",
    hdrs = ["VoiceNonce.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "pending_join_intent_store",
    hdrs = ["PendingJoinIntentStore.h"],
    srcs = ["PendingJoinIntentStore.cpp"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//app/managers/session:session_manager",
        "//domains/ids:domain_ids",
        "//infra/redis:redis_client",
        "//utils:loggable",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "voice_resume_registry",
    hdrs = ["VoiceResumeRegistry.h"],
    srcs = ["VoiceResumeRegistry.cpp"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":voice_nonce",
        "//domains/ids:domain_ids",
        "//infra/redis:redis_client",
        "//utils:loggable",
    ],
)

cc_library(
    name = "voice_publisher",
    hdrs = ["VoicePublisher.h"],
    srcs = ["VoicePublisher.cpp"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":voice_resume_registry",
        ":voice_session_manager",
        "//app/commands:utils",
        "//app/managers/session:session_manager",
        "//app/managers/subscription:subscription_manager",
        "//app/services/hub:hub_service",
        "//domains/ids:domain_ids",
        "//net/outbound:i_outbound_sink",
        "//proto:envelope_cc_proto",
        "//proto/event:activity_cc_proto",
        "//proto/event:state_cc_proto",
        "//utils:metrics",
    ],
)

cc_library(
    name = "voice_reconciler",
    hdrs = ["VoiceReconciler.h"],
    srcs = ["VoiceReconciler.cpp"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        ":reconcile_evidence_tracker",
        "//domains/ids:domain_ids",
        "//utils:env_loader",
        "//utils:loggable",
    ],
)

cc_library(
    name = "channel_key_service",
    hdrs = ["ChannelKeyService.h"],
    srcs = ["ChannelKeyService.cpp"],
    copts = ["-std=gnu++23"],
    linkopts = ["-lcrypto"],
    visibility = ["//visibility:public"],
    deps = [
        ":voice_publisher",
        ":voice_session_manager",
        "//app/services/hub:hub_service",
        "//domains/ids:domain_ids",
        "//infra/redis:redis_client",
        "//livekit/cli:livekit_client",
        "//livekit/crypto:livekit_e2ee_key_manager",
        "//livekit/routing:livekit_node_registry",
        "//livekit/token:livekit_token_service",
        "//utils:env_loader",
        "//utils:loggable",
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
    deps = [
        "//app/commands:utils",
        "//domains/ids:domain_ids",
        "//infra/persistence:persistence_gateway_lib",
        "//infra/redis:redis_client",
        "//app/managers/session:session_manager",
        "//app/managers/subscription:subscription_manager",
        "//app/services/hub:hub_service",
        "//core:livekit_cluster_config",
        ":voice_session_manager",
        ":voice_nonce",
        ":pending_join_intent_store",
        ":voice_resume_registry",
        ":voice_publisher",
        ":channel_key_service",
        ":voice_reconciler",
        "//livekit/token:livekit_token_service",
        "//livekit/routing:livekit_node_registry",
        "//livekit/cli:livekit_client",
        "//livekit/webhook:livekit_webhook_server",
        "//net/outbound:i_outbound_sink",
        "//utils:env_loader",
        "//utils:loggable",
        "//utils:metrics",
        "@nlohmann_json",
    ],
    visibility = ["//visibility:public"],
)
