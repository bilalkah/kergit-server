load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("//third_party:versions.bzl", "NANOID_VERSION")

def _nanoid_impl(ctx):
    git_repository(
        name = "nanoid",
        remote = "https://github.com/mykolamor/nanoid_cpp.git",
        commit = "{}".format(NANOID_VERSION),
        init_submodules = False,
        build_file_content = r'''
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

cmake(
    name = "nanoid_build_dbg",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Debug",
    },
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = ["libnanoid.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

cmake(
    name = "nanoid_build_fast",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    },
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = ["libnanoid.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

cmake(
    name = "nanoid_build_opt",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release",
    },
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = ["libnanoid.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# Select build per compilation mode
alias(
    name = "nanoid_build",
    actual = select({
        ":is_dbg":       ":nanoid_build_dbg",
        ":is_fastbuild": ":nanoid_build_fast",
        ":is_opt":       ":nanoid_build_opt",
        "//conditions:default": ":nanoid_build_fast",  # default -> RelWithDebInfo
    }),
    visibility = ["//visibility:public"],
)

# Public handle
alias(
    name = "nanoid",
    actual = ":nanoid_build",
    visibility = ["//visibility:public"],
)
''',
    )

nanoid = module_extension(
    implementation = _nanoid_impl,
)
