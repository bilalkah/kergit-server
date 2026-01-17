load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")

proto_library(
    name = "session_proto",
    srcs = ["session.proto"],
    deps = [
        "//proto/domain:channel_proto",
        "//proto/domain:hub_proto",
        "//proto/domain:user_proto",
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
    deps = [
        "//proto/domain:hub_proto",
    ],
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
