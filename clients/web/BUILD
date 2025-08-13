# Bazel build file for web client

load("@rules_python//python:defs.bzl", "py_binary")

# Export server.py for Docker build
exports_files(["server.py"])

py_binary(
    name = "web_server",
    srcs = ["server.py"],
    data = [
        "debug.html",
        "index.html",
        "src/app.js",
        "src/main.js",
        "styles.css",
        "test_connection.html",
    ],
    main = "server.py",
)

filegroup(
    name = "static_files",
    srcs = [
        "debug.html",
        "index.html",
        "src/app.js",
        "src/main.js",
        "styles.css",
        "test_connection.html",
    ],
)
