load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "public_id_service",
    srcs = ["PublicIdService.cpp"],
    hdrs = ["PublicIdService.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)
