load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party:versions.bzl", "LIBPQXX_SHA256", "LIBPQXX_VERSION")

def _libpqxx_impl(ctx):
    http_archive(
        name = "libpqxx",
        url = "https://github.com/jtv/libpqxx/archive/refs/tags/{}.tar.gz".format(LIBPQXX_VERSION),
        strip_prefix = "libpqxx-{}".format(LIBPQXX_VERSION),
        sha256 = LIBPQXX_SHA256,
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
