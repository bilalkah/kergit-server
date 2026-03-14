load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

proto_library(
    name = "user_proto",
    srcs = ["user.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "user_cc_proto",
    deps = [":user_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "ref_proto",
    srcs = ["ref.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "ref_cc_proto",
    deps = [":ref_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "channel_proto",
    srcs = ["channel.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "channel_cc_proto",
    deps = [":channel_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "hub_proto",
    srcs = ["hub.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "hub_cc_proto",
    deps = [":hub_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "message_proto",
    srcs = ["message.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "message_cc_proto",
    deps = [":message_proto"],
    visibility = ["//visibility:public"],
)
