# Bazel build file for web client

load("@rules_python//python:defs.bzl", "py_binary")

# Export server.py for Docker build
exports_files(["server.py"])

py_binary(
    name = "web_server",
    srcs = ["server.py"],
    data = [
        "index.html",
        "styles.css",
        "debug.html",
        "test_connection.html",
        "src/app.js",
        "src/main.js",
    ],
    main = "server.py",
)

filegroup(
    name = "static_files",
    srcs = [
        "index.html",
        "styles.css",
        "debug.html",
        "test_connection.html",
        "src/app.js",
        "src/main.js",
    ],
)
