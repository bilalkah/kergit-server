load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

proto_library(
    name = "session_proto",
    srcs = ["session.proto"],
    deps = [
        "//proto/domain:user_proto",
    ],
)

cc_proto_library(
    name = "session_cc_proto",
    deps = [":session_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "activity_proto",
    srcs = ["activity.proto"],
)

cc_proto_library(
    name = "activity_cc_proto",
    deps = [":activity_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "message_proto",
    srcs = ["message.proto"],
)

cc_proto_library(
    name = "message_cc_proto",
    deps = [":message_proto"],
    visibility = ["//visibility:public"],
)


proto_library(
    name = "hub_proto",
    srcs = ["hub.proto"],
)

cc_proto_library(
    name = "hub_cc_proto",
    deps = [":hub_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "user_proto",
    srcs = ["user.proto"],
)

cc_proto_library(
    name = "user_cc_proto",
    deps = [":user_proto"],
    visibility = ["//visibility:public"],
)
