load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _libpqxx_impl(ctx):
    http_archive(
        name = "libpqxx",
        url = "https://github.com/jtv/libpqxx/archive/refs/tags/7.10.1.tar.gz",
        strip_prefix = "libpqxx-7.10.1",
        sha256 = "cfbbb1d93a0a3d81319ec71d9a3db80447bb033c4f6cee088554a88862fd77d7",
        build_file_content = """
load("@rules_foreign_cc//foreign_cc:configure.bzl", "configure_make")

configure_make(
    name = "libpqxx_build",
    configure_options = [
        "--disable-documentation",
        "--enable-shared",
        "--disable-static",
    ],
    copts = [
        "-fPIC",
    ],
    linkopts = [
        "-lpq",
    ],
    lib_source = ":all_srcs",

    out_lib_dir = "lib",
    out_shared_libs = [
        "libpqxx.so",
        "libpqxx-7.10.so",
    ],

    targets = ["install"],
    out_include_dir = "include",
    out_bin_dir = "bin",
    visibility = ["//visibility:public"],
)

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

alias(
    name = "libpqxx",
    actual = ":libpqxx_build",
    visibility = ["//visibility:public"],
)
""",
    )

libpqxx = module_extension(
    implementation = _libpqxx_impl,
)
