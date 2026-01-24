load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "hub_cache",
    srcs = ["HubCache.cpp"],
    hdrs = ["HubCache.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//core/cache:on_memory_cache",
        "//domains:hub_domain",
    ],
)

cc_library(
    name = "hub_service",
    srcs = ["HubService.cpp"],
    hdrs = [
        "HubService.h",
        "HubSnapshot.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        ":hub_cache",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//infra/persistence:persistence_gateway_lib",
        "//utils:loggable",
    ],
)

cc_library(
    name = "hub_snapshot_builder",
    srcs = ["SnapshotBuilder.cpp"],
    hdrs = ["SnapshotBuilder.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        ":hub_service",
        "//app/services:public_id_service",
        "//app/services/channel:channel_service",
        "//app/services/presence:presence_service",
        "//domains:channel_domain",
        "//domains/ids:domain_ids",
        "@nlohmann_json",
    ],
)

cc_library(
    name = "hub_notifier",
    srcs = ["HubNotifier.cpp"],
    hdrs = ["HubNotifier.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//app/services:public_id_service",
        "//domains/ids:domain_ids",
        "//proto:envelope_cc_proto",
        "//proto/event:presence_cc_proto",
        "@nlohmann_json",
    ],
)
