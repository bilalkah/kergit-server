load("@rules_cc//cc:defs.bzl", "cc_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

proto_library(
    name = "envelope_proto",
    srcs = ["envelope.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "envelope_cc_proto",
    deps = [":envelope_proto"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "parsed_payload",
    hdrs = ["ParsedPayload.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//proto/command:activity_cc_proto",
        "//proto/command:channel_cc_proto",
        "//proto/command:hub_cc_proto",
        "//proto/command:message_cc_proto",
        "//proto/command:session_cc_proto",
        "//proto/command:user_cc_proto",
        "//proto/system:heartbeat_cc_proto",
    ],
)
