load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

proto_library(
    name = "envelope_proto",
    srcs = ["envelope.proto"],
)

cc_proto_library(
    name = "envelope_cc_proto",
    deps = [":envelope_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "chat_proto",
    srcs = ["chat.proto"],
)

cc_proto_library(
    name = "chat_cc_proto",
    deps = [":chat_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "heartbeat_proto",
    srcs = ["heartbeat.proto"],
)

cc_proto_library(
    name = "heartbeat_cc_proto",
    deps = [":heartbeat_proto"],
    visibility = ["//visibility:public"],
)
