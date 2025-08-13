load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

# file group for core/database/sql/database_schema.sql
filegroup(
    name = "database_schema_sql",
    srcs = glob(["sql/*.sql"]),
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "chatdb_init",
    srcs = [
        "src/chatdb_init.cpp",
        "src/chatdb_init.h",
    ],
    deps = [
        "//common",
        "@libpqxx",
        "@nlohmann_json",
    ],
)

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
