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
    name = "redis_plus_plus_build_dbg",
    cache_entries = {
        "CMAKE_BUILD_TYPE":             "Debug",
        "CMAKE_PREFIX_PATH":            "$$EXT_BUILD_ROOT/bazel-out/k8-dbg/bin/external/+hiredis+hiredis/hiredis_build_dbg",
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

# ---- fastbuild (RelWithDebInfo) ----
# Problematic with imported config mapping, so release will be used instead
cmake(
    name = "redis_plus_plus_build_fast",
    cache_entries = {
        "CMAKE_BUILD_TYPE":             "RelWithDebInfo",
        "CMAKE_PREFIX_PATH":            "$$EXT_BUILD_ROOT/bazel-out/k8-fastbuild/bin/external/+hiredis+hiredis/hiredis_build_fast",
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

# ---- opt (Release) ----
cmake(
    name = "redis_plus_plus_build_opt",
    cache_entries = {
        "CMAKE_BUILD_TYPE":             "Release",
        "CMAKE_PREFIX_PATH":            "$$EXT_BUILD_ROOT/bazel-out/k8-opt/bin/external/+hiredis+hiredis/hiredis_build_opt",
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

# Select the right one per --compilation_mode
alias(
    name = "redis_plus_plus_build",
    actual = select({
        ":is_dbg": ":redis_plus_plus_build_dbg",
        # ":is_fastbuild": ":redis_plus_plus_build_fast", --- Problematic with imported config mapping, so release will be used instead
        ":is_opt": ":redis_plus_plus_build_opt",
        "//conditions:default": ":redis_plus_plus_build_opt",
    }),
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
