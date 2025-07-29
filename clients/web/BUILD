# Bazel build file for web client

load("@rules_python//python:defs.bzl", "py_binary")

py_binary(
    name = "web_server",
    srcs = ["server.py"],
    data = [
        "index.html",
        "app.js",
        "styles.css",
        "debug.html",
        "test_connection.html",
    ],
    main = "server.py",
)

filegroup(
    name = "static_files",
    srcs = [
        "index.html",
        "app.js",
        "styles.css",
        "debug.html",
        "test_connection.html",
    ],
)