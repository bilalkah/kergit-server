load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

proto_library(
    name = "auth_proto",
    srcs = ["auth.proto"],
    deps = [
        "//proto/domain:user_proto",
    ],
)

cc_proto_library(
    name = "auth_cc_proto",
    deps = [":auth_proto"],
    visibility = ["//visibility:public"],
)
