load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party:versions.bzl", "NLOHMANN_JSON_SHA256", "NLOHMANN_JSON_VERSION")

def _nlohmann_json_impl(ctx):
    http_archive(
        name = "nlohmann_json",
        url = "https://github.com/nlohmann/json/releases/download/{}/json.tar.xz".format(NLOHMANN_JSON_VERSION),
        strip_prefix = "json",
        sha256 = NLOHMANN_JSON_SHA256,
        build_file_content = """
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "nlohmann_json",
    hdrs = glob(["single_include/nlohmann/**/*.hpp"]),
    includes = ["single_include"],
    visibility = ["//visibility:public"],
)
""",
    )

nlohmann_json = module_extension(
    implementation = _nlohmann_json_impl,
)
