load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party:versions.bzl", "HIREDIS_SHA256", "HIREDIS_VERSION")

def _hiredis_impl(ctx):
    http_archive(
        name = "hiredis",
        url = "https://github.com/redis/hiredis/archive/refs/tags/v{}.tar.gz".format(HIREDIS_VERSION),
        strip_prefix = "hiredis-{}".format(HIREDIS_VERSION),
        sha256 = HIREDIS_SHA256,
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

# ---- Debug ----
cmake(
    name = "hiredis_build_dbg",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Debug",
        "ENABLE_SSL": "ON",
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

# ---- Fastbuild (RelWithDebInfo) ----
cmake(
    name = "hiredis_build_fast",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "ENABLE_SSL": "ON",
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

# ---- Opt (Release) ----
cmake(
    name = "hiredis_build_opt",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release",
        "ENABLE_SSL": "ON",
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

# Mode selector (alias)
alias(
    name = "hiredis_build",
    actual = select({
        ":is_dbg": ":hiredis_build_dbg",
        # ":is_fastbuild": ":hiredis_build_fast", --- Problematic with imported config mapping, so release will be used instead
        ":is_opt": ":hiredis_build_opt",
        "//conditions:default": ":hiredis_build_opt",
    }),
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
