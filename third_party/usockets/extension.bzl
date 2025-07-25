load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _usockets_impl(ctx):
    http_archive(
        name = "usockets",
        url = "https://github.com/uNetworking/uSockets/archive/refs/tags/v0.8.6.tar.gz",
        strip_prefix = "uSockets-0.8.6",
        build_file = "//third_party/usockets:BUILD.bazel",
    )

usockets = module_extension(
    implementation = _usockets_impl,
) 