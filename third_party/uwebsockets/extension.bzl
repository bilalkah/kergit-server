load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _uwebsockets_impl(ctx):
    http_archive(
        name = "uwebsockets",
        url = "https://github.com/uNetworking/uWebSockets/archive/refs/tags/v20.62.0.tar.gz",
        strip_prefix = "uWebSockets-20.62.0",
        build_file = "//third_party/uwebsockets:BUILD.bazel",
    )

uwebsockets = module_extension(
    implementation = _uwebsockets_impl,
) 