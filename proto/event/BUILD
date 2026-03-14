load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")

proto_library(
    name = "activity_proto",
    srcs = ["activity.proto"],
    deps = [
        "//proto/domain:ref_proto",
    ],
)

cc_proto_library(
    name = "activity_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":activity_proto"],
)

proto_library(
    name = "session_proto",
    srcs = ["session.proto"],
)

cc_proto_library(
    name = "session_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":session_proto"],
)

proto_library(
    name = "heartbeat_proto",
    srcs = ["heartbeat.proto"],
)

cc_proto_library(
    name = "heartbeat_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":heartbeat_proto"],
)

proto_library(
    name = "state_proto",
    srcs = ["state.proto"],
    deps = [
        "//proto/domain:channel_proto",
        "//proto/domain:hub_proto",
        "//proto/domain:message_proto",
        "//proto/domain:user_proto",
    ],
)

cc_proto_library(
    name = "state_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":state_proto"],
)

proto_library(
    name = "realtime_proto",
    srcs = ["realtime.proto"],
    deps = [
        "//proto/domain:ref_proto",
    ],
)

cc_proto_library(
    name = "realtime_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":realtime_proto"],
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
