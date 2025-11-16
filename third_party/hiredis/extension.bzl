load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party:versions.bzl", "HIREDIS_VERSION", "HIREDIS_SHA256")

def _hiredis_impl(ctx):
    http_archive(
        name = "hiredis",
        url = "https://github.com/redis/hiredis/archive/refs/tags/v{}.tar.gz".format(HIREDIS_VERSION),
        strip_prefix = "hiredis-{}".format(HIREDIS_VERSION),
        sha256 = HIREDIS_SHA256,
        build_file_content = """
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "hiredis_build",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release",
        "ENABLE_SSL": "OFF",
        "BUILD_TESTING": "OFF",
        "BUILD_SHARED_LIBS": "OFF",
    },
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = [
        "libhiredis.a",
    ],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

alias(
    name = "hiredis",
    actual = ":hiredis_build",
    visibility = ["//visibility:public"],
)

""",
    )

hiredis = module_extension(
    implementation = _hiredis_impl,
)
