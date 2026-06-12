"""Bazel module extension: builds libdeflate with rules_foreign_cc (CMake)."""
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def _libdeflate_impl(ctx):
    git_repository(
        name = "libdeflate",
        remote = "https://github.com/ebiggers/libdeflate.git",
        commit = "8d351ab307e91f5de980215c6fea2816374f93e8",
        init_submodules = False,
        build_file_content = r"""
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

# --- modes ---
config_setting(name = "is_dbg",       values = {"compilation_mode": "dbg"})
config_setting(name = "is_fastbuild", values = {"compilation_mode": "fastbuild"})
config_setting(name = "is_opt",       values = {"compilation_mode": "opt"})

# Expose all sources mainly for "cmake(lib_source=...)".
filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

# ---- Debug build ----
cmake(
    name = "libdeflate_build_dbg",
    lib_source = ":all_srcs",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Debug",
        "LIBDEFLATE_BUILD_SHARED_LIB": "OFF",
        "LIBDEFLATE_BUILD_PROGRAMS": "OFF",
    },
    out_lib_dir = "lib",
    out_static_libs = ["libdeflate.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# ---- Fastbuild (RelWithDebInfo) ----
cmake(
    name = "libdeflate_build_fast",
    lib_source = ":all_srcs",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "LIBDEFLATE_BUILD_SHARED_LIB": "OFF",
        "LIBDEFLATE_BUILD_PROGRAMS": "OFF",
    },
    out_lib_dir = "lib",
    out_static_libs = ["libdeflate.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# ---- Opt (Release) ----
cmake(
    name = "libdeflate_build_opt",
    lib_source = ":all_srcs",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release",
        "LIBDEFLATE_BUILD_SHARED_LIB": "OFF",
        "LIBDEFLATE_BUILD_PROGRAMS": "OFF",
    },
    out_lib_dir = "lib",
    out_static_libs = ["libdeflate.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# Select build per compilation mode
alias(
    name = "libdeflate_build",
    actual = select({
        ":is_dbg":       ":libdeflate_build_dbg",
        ":is_fastbuild": ":libdeflate_build_fast",
        ":is_opt":       ":libdeflate_build_opt",
        "//conditions:default": ":libdeflate_build_fast",  # default -> RelWithDebInfo
    }),
    visibility = ["//visibility:public"],
)

# Public handle
alias(
    name = "libdeflate",
    actual = ":libdeflate_build",
    visibility = ["//visibility:public"],
)
""",
    )

libdeflate = module_extension(
    implementation = _libdeflate_impl,
)
