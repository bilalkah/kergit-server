load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _uwebsockets_impl(ctx):
    http_archive(
        name = "uwebsockets",
        url = "https://github.com/uNetworking/uWebSockets/archive/refs/tags/v20.62.0.tar.gz",
        strip_prefix = "uWebSockets-20.62.0",
        build_file_content = """
cc_library(
    name = "uwebsockets",
    srcs = [
        "capi/libuwebsockets.cpp"
    ],
    hdrs = glob([
        "src/*.h",
        "capi/libuwebsockets.h"
    ]),
    includes = ["src", "capi", "."],
    copts = [
        "-DLIBUS_NO_SSL",
        "-std=c++17",
    ],
    linkopts = ["-lpthread", "-lz"],
    visibility = ["//visibility:public"],
    deps = ["@usockets//:usockets"],
)
"""
    )

uwebsockets = module_extension(
    implementation = _uwebsockets_impl,
) 