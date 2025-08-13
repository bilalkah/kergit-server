load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

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
        "@nlohmann_json",
        "@libpqxx",
    ],
)

cc_library(
    name = "chatdb_lib",
    srcs = ["src/chatdb.cpp"],
    hdrs = ["src/chatdb.h"],
    deps = [
        "//common",
        "@libpqxx",
        "@nlohmann_json",
    ],
    visibility = ["//visibility:public"],
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

