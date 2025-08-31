# Bazel build file for web client
load("@rules_python//python:defs.bzl", "py_binary")

# Tool to turn .env -> JS module
py_binary(
    name = "gen_config",
    srcs = ["gen_config.py"],
)

# Generate src/config.js (no shell quoting headaches)
genrule(
    name = "config_js",
    srcs = [".env"],
    outs = ["src/config.js"],
    cmd = "$(location :gen_config) $(location .env) > $@",
    tools = [":gen_config"],
)

filegroup(
    name = "static_files",
    srcs = [
        "index.html",
        "src/app.js",
        "src/auth/supabase.js",
        "src/main.js",
        "src/net/ws.js",
        "src/ui/chat.js",
        "src/ui/dom.js",
        "src/ui/login.js",
        "src/ui/status.js",
        "styles.css",
        ":config_js",
    ],
)

py_binary(
    name = "web_server",
    srcs = ["server.py"],
    data = [
        ":static_files",
    ],
    main = "server.py",
)
