load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "invite_service",
    srcs = ["InviteService.cpp"],
    hdrs = ["InviteService.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "//infra/redis:redis_client",
    ],
)
