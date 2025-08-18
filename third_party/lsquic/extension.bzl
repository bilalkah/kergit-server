load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("//third_party:versions.bzl", "LSQUIC_VERSION")

def _lsquic_impl(ctx):
    git_repository(
        name = "lsquic",  # external repo name
        remote = "https://github.com/litespeedtech/lsquic.git",
        tag = "{}".format(LSQUIC_VERSION), 
        init_submodules = True,  # clone submodules
        recursive_init_submodules = True,  # if your Bazel has it
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

# ---- dbg ----
cmake(
    name = "lsquic_build_dbg",
    cache_entries = {
        "CMAKE_BUILD_TYPE":"Debug",
        "LSQUIC_TESTS":"OFF",
        "BORINGSSL_LIB":     "$$EXT_BUILD_ROOT/bazel-out/k8-dbg/bin/external/+boringssl+boringssl/boringssl_build_dbg/lib",
        "BORINGSSL_INCLUDE": "$$EXT_BUILD_ROOT/bazel-out/k8-dbg/bin/external/+boringssl+boringssl/boringssl_build_dbg/include",
    },
    deps = ["@boringssl//:boringssl"],
    includes = ["@boringssl//:boringssl"],
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = ["liblsquic.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# ---- fastbuild (RelWithDebInfo) ----
cmake(
    name = "lsquic_build_fast",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "LSQUIC_TESTS": "OFF",
        "BORINGSSL_LIB":     "$$EXT_BUILD_ROOT/bazel-out/k8-fastbuild/bin/external/+boringssl+boringssl/boringssl_build_fast/lib",
        "BORINGSSL_INCLUDE": "$$EXT_BUILD_ROOT/bazel-out/k8-fastbuild/bin/external/+boringssl+boringssl/boringssl_build_fast/include",
    },
    deps = ["@boringssl//:boringssl"],
    includes = ["@boringssl//:boringssl"],
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = ["liblsquic.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# ---- opt (Release) ----
cmake(
    name = "lsquic_build_opt",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release",
        "LSQUIC_TESTS": "OFF",
        "BORINGSSL_LIB":     "$$EXT_BUILD_ROOT/bazel-out/k8-opt/bin/external/+boringssl+boringssl/boringssl_build_opt/lib",
        "BORINGSSL_INCLUDE": "$$EXT_BUILD_ROOT/bazel-out/k8-opt/bin/external/+boringssl+boringssl/boringssl_build_opt/include",
    },
    deps = ["@boringssl//:boringssl"],
    includes = ["@boringssl//:boringssl"],
    lib_source = ":all_srcs",
    out_lib_dir = "lib",
    out_static_libs = ["liblsquic.a"],
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

# Select the right one per --compilation_mode
alias(
    name = "lsquic_build",
    actual = select({
        ":is_dbg":       ":lsquic_build_dbg",
        ":is_fastbuild": ":lsquic_build_fast",
        ":is_opt":       ":lsquic_build_opt",
        "//conditions:default": ":lsquic_build_fast",  # default -> RelWithDebInfo
    }),
    visibility = ["//visibility:public"],
)

alias(
    name = "lsquic",
    actual = ":lsquic_build",
    visibility = ["//visibility:public"],
)

""",
    )

lsquic = module_extension(
    implementation = _lsquic_impl,
)
