load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _nlohmann_json_impl(ctx):
    http_archive(
        name = "nlohmann_json",
        url = "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz",
        strip_prefix = "json",
        build_file_content = """
cc_library(
    name = "nlohmann_json",
    hdrs = glob(["single_include/nlohmann/**/*.hpp"]),
    includes = ["single_include"],
    visibility = ["//visibility:public"],
)
"""
    )

nlohmann_json = module_extension(
    implementation = _nlohmann_json_impl,
)
