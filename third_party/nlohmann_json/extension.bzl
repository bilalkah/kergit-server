load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _nlohmann_json_impl(ctx):
    http_archive(
        name = "nlohmann_json",
        url = "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz",
        strip_prefix = "json",
        build_file = "//third_party/nlohmann_json:BUILD.bazel",
    )

nlohmann_json = module_extension(
    implementation = _nlohmann_json_impl,
) 