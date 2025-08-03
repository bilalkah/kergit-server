# Bazel build file for web client

load("@rules_python//python:defs.bzl", "py_binary")

py_binary(
    name = "web_server",
    srcs = ["server.py"],
    data = [
        "app.js",
        "debug.html",
        "index.html",
        "styles.css",
        "test_connection.html",
    ],
    main = "server.py",
)

filegroup(
    name = "static_files",
    srcs = [
        "app.js",
        "debug.html",
        "index.html",
        "styles.css",
        "test_connection.html",
    ],
)
