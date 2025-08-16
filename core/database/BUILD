load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")

sh_binary(
    name = "initialize_database",
    srcs = ["scripts/dev_db_init.sh"],
    data = ["//core/database/sql:V001__init.sql"],
    args = ["$(location //core/database/sql:V001__init.sql)"],
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
