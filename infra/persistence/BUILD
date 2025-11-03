load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "chatdb_lib",
    srcs = ["chatdb.cpp"],
    hdrs = ["chatdb.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
        "@libpqxx",
        "@nlohmann_json",
    ],
)
