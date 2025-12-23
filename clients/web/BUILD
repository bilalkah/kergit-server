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
    srcs = ["//:.env"],
    outs = ["src/config.js"],
    cmd = "$(location :gen_config) $(location //:.env) > $@",
    tools = [":gen_config"],
)

filegroup(
    name = "static_files",
    srcs = [
        "index.html",
        "src/api/chatApi.js",
        "src/controllers/authController.js",
        "src/controllers/channelController.js",
        "src/controllers/hubController.js",
        "src/controllers/hubManagementController.js",
        "src/controllers/listController.js",
        "src/controllers/messageController.js",
        "src/controllers/profileController.js",
        "src/controllers/realtime.js",
        "src/main.js",
        "src/services/supabase.js",
        "src/services/ws.js",
        "src/store/selectors.js",
        "src/store/state.js",
        "src/views/chat.js",
        "src/views/sidebar.js",
        "src/views/status.js",
        "styles.css",
        ":config_js",
    ],
)

# filegroup(
#     name = "static_files",
#     srcs = [
#         "index.html",
#         "src/app.js",
#         "src/auth/supabase.js",
#         "src/main.js",
#         "src/net/ws.js",
#         "src/ui/chat.js",
#         "src/ui/dom.js",
#         "src/ui/login.js",
#         "src/ui/status.js",
#         "styles.css",
#         ":config_js",
#     ],
# )

py_binary(
    name = "http_server",
    srcs = ["http_server.py"],
    data = [
        ":static_files",
    ],
    main = "http_server.py",
)

py_binary(
    name = "https_server",
    srcs = ["https_server.py"],
    data = [
        ":static_files",
        "//certs:cert.pem",
        "//certs:key.pem",
    ],
    main = "https_server.py",
)
