load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party:versions.bzl", "JWT_CPP_SHA256", "JWT_CPP_VERSION")

def _jwt_cpp_impl(ctx):
    http_archive(
        name = "jwt-cpp",
        url = "https://github.com/Thalhammer/jwt-cpp/releases/download/{}/jwt-cpp-{}.zip".format(
            JWT_CPP_VERSION,
            JWT_CPP_VERSION,
        ),
        sha256 = JWT_CPP_SHA256,
        build_file_content = '''
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
    name = "jwt_cpp_build_dbg",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Debug",
        "JWT_BUILD_EXAMPLES": "OFF",
        "JWT_BUILD_TESTS": "OFF",
        "JWT_BUILD_DOCS": "OFF",
        "JWT_ENABLE_COVERAGE": "OFF",
        "JWT_ENABLE_FUZZING": "OFF",
    },
    lib_source = ":all_srcs",
    out_headers_only = True,
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

cmake(
    name = "jwt_cpp_build_fast",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "JWT_BUILD_EXAMPLES": "OFF",
        "JWT_BUILD_TESTS": "OFF",
        "JWT_BUILD_DOCS": "OFF",
        "JWT_ENABLE_COVERAGE": "OFF",
        "JWT_ENABLE_FUZZING": "OFF",
    },
    lib_source = ":all_srcs",
    out_headers_only = True,
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

cmake(
    name = "jwt_cpp_build_opt",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release",
        "JWT_BUILD_EXAMPLES": "OFF",
        "JWT_BUILD_TESTS": "OFF",
        "JWT_BUILD_DOCS": "OFF",
        "JWT_ENABLE_COVERAGE": "OFF",
        "JWT_ENABLE_FUZZING": "OFF",
    },
    lib_source = ":all_srcs",
    out_headers_only = True,
    out_include_dir = "include",
    visibility = ["//visibility:public"],
)

alias(
    name = "jwt_cpp_build",
    actual = select({
        ":is_dbg":       ":jwt_cpp_build_dbg",
        ":is_fastbuild": ":jwt_cpp_build_fast",
        ":is_opt":       ":jwt_cpp_build_opt",
        "//conditions:default": ":jwt_cpp_build_fast",
    }),
    visibility = ["//visibility:public"],
)

alias(
    name = "jwt-cpp",
    actual = ":jwt_cpp_build",
    visibility = ["//visibility:public"],
)
''',
    )

jwt_cpp = module_extension(
    implementation = _jwt_cpp_impl,
)
