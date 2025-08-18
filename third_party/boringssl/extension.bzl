load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party:versions.bzl", "BORINGSSL_SHA256", "BORINGSSL_VERSION")

def _boringssl_impl(ctx):
    http_archive(
        name = "boringssl",
        url = "https://github.com/google/boringssl/archive/refs/tags/{}.tar.gz".format(BORINGSSL_VERSION),
        strip_prefix = "boringssl-{}".format(BORINGSSL_VERSION),
        sha256 = BORINGSSL_SHA256,
        build_file_content = """
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

# --- modes ---
config_setting(name = "is_dbg",       values = {"compilation_mode": "dbg"})
config_setting(name = "is_fastbuild", values = {"compilation_mode": "fastbuild"})
config_setting(name = "is_opt",       values = {"compilation_mode": "opt"})

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "include",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)

# ---- Debug ----
cmake(
    name = "boringssl_build_dbg",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Debug",
        "BUILD_SHARED_LIBS": "OFF",
        "BUILD_TESTING": "OFF",
    },
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = [
        "libssl.a",
        "libcrypto.a",
    ],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# ---- Fastbuild (RelWithDebInfo) ----
cmake(
    name = "boringssl_build_fast",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "BUILD_SHARED_LIBS": "OFF",
        "BUILD_TESTING": "OFF",
    },
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = [
        "libssl.a",
        "libcrypto.a",
    ],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# ---- Opt (Release) ----
cmake(
    name = "boringssl_build_opt",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release",
        "BUILD_SHARED_LIBS": "OFF",
        "BUILD_TESTING": "OFF",
    },
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = ["libssl.a", "libcrypto.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# Mode selector (alias)
alias(
    name = "boringssl_build",
    actual = select({
        ":is_dbg":       ":boringssl_build_dbg",   # CMAKE_BUILD_TYPE=Debug
        ":is_fastbuild": ":boringssl_build_fast",  # CMAKE_BUILD_TYPE=RelWithDebInfo
        ":is_opt":       ":boringssl_build_opt",   # CMAKE_BUILD_TYPE=Release
        "//conditions:default": ":boringssl_build_fast",  # safety fallback
    }),
    visibility = ["//visibility:public"],
)

alias(
    name = "boringssl",
    actual = ":boringssl_build",
    visibility = ["//visibility:public"],
)
""",
    )

boringssl = module_extension(
    implementation = _boringssl_impl,
)
