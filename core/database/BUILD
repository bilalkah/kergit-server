load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "chatdb_lib",
    srcs = ["src/chatdb.cpp"],
    hdrs = ["src/chatdb.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//common",
        "@libpqxx",
        "@nlohmann_json",
    ],
)

cc_test(
    name = "chatdb_test",
    srcs = ["test/database_connection.cpp"],
    deps = [
        ":chatdb_lib",
        "//common",
        "@googletest//:gtest_main",
    ],
)
