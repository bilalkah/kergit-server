load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "proto_converters",
    hdrs = ["ProtoConverters.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//proto/domain:channel_cc_proto",
        "//proto/domain:hub_cc_proto",
    ],
)
