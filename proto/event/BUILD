load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")

proto_library(
    name = "activity_proto",
    srcs = ["activity.proto"],
)

cc_proto_library(
    name = "activity_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":activity_proto"],
)

proto_library(
    name = "session_proto",
    srcs = ["session.proto"],
    deps = [
        "//proto/domain:hub_proto",
        "//proto/domain:user_proto",
        ":activity_proto",
    ],
)

cc_proto_library(
    name = "session_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":session_proto"],
)

proto_library(
    name = "presence_proto",
    srcs = ["presence.proto"],
)

cc_proto_library(
    name = "presence_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":presence_proto"],
)

proto_library(
    name = "error_proto",
    srcs = ["error.proto"],
    deps = [
        "//proto:envelope_proto",
    ],
)

cc_proto_library(
    name = "error_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":error_proto"],
)

proto_library(
    name = "message_proto",
    srcs = ["message.proto"],
    deps = [
        "//proto/domain:message_proto",
    ],
)

cc_proto_library(
    name = "message_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":message_proto"],
)

proto_library(
    name = "hub_proto",
    srcs = ["hub.proto"],
    deps = [
        "//proto/domain:channel_proto",
        "//proto/domain:hub_proto",
        "//proto/domain:user_proto",
    ],
)

cc_proto_library(
    name = "hub_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":hub_proto"],
)

proto_library(
    name = "channel_proto",
    srcs = ["channel.proto"],
    deps = [
        "//proto/domain:channel_proto",
    ],
)

cc_proto_library(
    name = "channel_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":channel_proto"],
)

proto_library(
    name = "user_proto",
    srcs = ["user.proto"],
    deps = [
        "//proto/domain:user_proto",
    ],
)

cc_proto_library(
    name = "user_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":user_proto"],
)
