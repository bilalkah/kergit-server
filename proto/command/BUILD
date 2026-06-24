load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

proto_library(
    name = "session_proto",
    srcs = ["session.proto"],
    deps = [
        "//proto/domain:ref_proto",
    ],
)

cc_proto_library(
    name = "session_cc_proto",
    deps = [":session_proto"],
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

proto_library(
    name = "ack_proto",
    srcs = ["ack.proto"],
)

cc_proto_library(
    name = "ack_cc_proto",
    deps = [":ack_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "activity_proto",
    srcs = ["activity.proto"],
    deps = [
        "//proto/domain:message_proto",
        "//proto/domain:ref_proto",
    ],
)

cc_proto_library(
    name = "activity_cc_proto",
    deps = [":activity_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "channel_proto",
    srcs = ["channel.proto"],
    deps = [
        "//proto/domain:channel_proto",
        "//proto/domain:ref_proto",
    ],
)

cc_proto_library(
    name = "channel_cc_proto",
    deps = [":channel_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "message_proto",
    srcs = ["message.proto"],
    deps = [
        "//proto/domain:message_proto",
        "//proto/domain:ref_proto",
    ],
)

cc_proto_library(
    name = "message_cc_proto",
    deps = [":message_proto"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "hub_proto",
    srcs = ["hub.proto"],
    deps = [
        "//proto/domain:hub_proto",
    ],
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
