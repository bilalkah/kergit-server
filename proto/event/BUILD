load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

proto_library(
    name = "bootstrap_proto",
    srcs = ["bootstrap.proto"],
    deps = [
        "//proto/domain:channel_proto",
        "//proto/domain:hub_proto",
        "//proto/domain:user_proto",
    ],
)

cc_proto_library(
    name = "bootstrap_cc_proto",
    deps = [":bootstrap_proto"],
    visibility = ["//visibility:public"],
)