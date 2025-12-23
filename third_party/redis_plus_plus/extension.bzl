load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party:versions.bzl", "REDIS_PLUS_PLUS_VERSION", "REDIS_PLUS_PLUS_SHA256")

def _redis_plus_plus_impl(ctx):
    http_archive(
        name = "redis_plus_plus",
        url = "https://github.com/sewenew/redis-plus-plus/archive/refs/tags/{}.tar.gz".format(REDIS_PLUS_PLUS_VERSION),
        strip_prefix = "redis-plus-plus-{}".format(REDIS_PLUS_PLUS_VERSION),
        sha256 = REDIS_PLUS_PLUS_SHA256,
        build_file_content = """
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")
filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)
cmake(
    name = "redis_plus_plus_build",
    cache_entries = {
        "CMAKE_BUILD_TYPE":"Release",
        "CMAKE_PREFIX_PATH":     "$$EXT_BUILD_ROOT/bazel-out/k8-dbg/bin/external/+hiredis+hiredis/hiredis_build",
        "CMAKE_PREFIX_PATH":     "$$EXT_BUILD_ROOT/bazel-out/aarch64-dbg/bin/external/+hiredis+hiredis/hiredis_build",
        "REDIS_PLUS_PLUS_BUILD_SHARED": "OFF",
    },
    deps = ["@hiredis//:hiredis"],
    includes = ["@hiredis//:hiredis"],
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = ["libredis++.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

alias(
    name = "redis_plus_plus",
    actual = ":redis_plus_plus_build",
    visibility = ["//visibility:public"],
)
""",
    )

redis_plus_plus = module_extension(
    implementation = _redis_plus_plus_impl,
)
