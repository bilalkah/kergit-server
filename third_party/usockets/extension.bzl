load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _usockets_impl(ctx):
    http_archive(
        name = "usockets",
        url = "https://github.com/uNetworking/uSockets/archive/refs/tags/v0.8.6.tar.gz",
        strip_prefix = "uSockets-0.8.6",
        build_file_content = """
cc_library(
    name = "usockets",
    srcs = glob([
        "src/*.c",
        "src/eventing/*.c",
        "src/io_uring/*.c",
        "src/crypto/*.c"
    ]),
    hdrs = glob([
        "src/*.h",
        "src/internal/*.h",
        "src/internal/eventing/*.h",
        "src/internal/networking/*.h"
    ]),
    includes = ["src", "src/internal", "src/internal/eventing", "src/internal/networking"],
    copts = ["-DLIBUS_NO_SSL"],
    visibility = ["//visibility:public"],
)
"""
    )

usockets = module_extension(
    implementation = _usockets_impl,
)
