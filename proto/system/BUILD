load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

proto_library(
    name = "heartbeat_proto",
    srcs = ["heartbeat.proto"],
)

cc_proto_library(
    name = "heartbeat_cc_proto",
    deps = [":heartbeat_proto"],
    visibility = ["//visibility:public"],
)