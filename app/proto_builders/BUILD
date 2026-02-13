load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "proto_builders",
    hdrs = [
        "EnvelopeBuilders.h",
        "PresenceBuilders.h",
        "VoiceBuilders.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//proto:envelope_cc_proto",
        "//proto/event:activity_cc_proto",
        "//proto/event:presence_cc_proto",
    ],
)
