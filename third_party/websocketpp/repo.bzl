load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def websocketpp_repository():
    http_archive(
        name = "websocketpp",
        url = "https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.tar.gz",
        strip_prefix = "websocketpp-0.8.2",
        build_file = "//third_party/websocketpp:BUILD.bazel",
    ) 