load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def uwebsockets_repository():
    http_archive(
        name = "uwebsockets",
        url = "https://github.com/uNetworking/uWebSockets/archive/refs/tags/v20.62.0.tar.gz",
        strip_prefix = "uWebSockets-20.62.0",
        build_file = "//third_party/uwebsockets:BUILD.bazel",
    ) 