"""Bazel extension for uWebSockets (http_archive)."""
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party:versions.bzl", "UWEBSOCKETS_VERSION", "UWEBSOCKETS_SHA256")

def _uwebsockets_impl(ctx):
    http_archive(
        name = "uwebsockets",
        url = "https://github.com/uNetworking/uWebSockets/archive/refs/tags/v{}.tar.gz".format(UWEBSOCKETS_VERSION),
        strip_prefix = "uWebSockets-{}".format(UWEBSOCKETS_VERSION),
        sha256 = "{}".format(UWEBSOCKETS_SHA256),
        build_file_content = r"""
# === Feature toggles (match upstream build.c env flags) ===
config_setting(name = "with_zlib",       define_values = {"WITH_ZLIB": "1"})
config_setting(name = "with_boringssl",  define_values = {"WITH_BORINGSSL": "1"})
config_setting(name = "with_openssl",    define_values = {"WITH_OPENSSL": "1"})
config_setting(name = "with_wolfssl",    define_values = {"WITH_WOLFSSL": "1"})
config_setting(name = "with_quic",       define_values = {"WITH_QUIC": "1"})
config_setting(name = "with_libuv",      define_values = {"WITH_LIBUV": "1"})
config_setting(name = "with_asio",       define_values = {"WITH_ASIO": "1"})
config_setting(name = "with_asan",       define_values = {"WITH_ASAN": "1"})
config_setting(name = "with_lto",        define_values = {"WITH_LTO": "1"})
config_setting(name = "with_libdeflate", define_values = {"WITH_LIBDEFLATE": "1"})

# Header-only: ONLY headers live under src/**
UWS_HDRS = glob(["src/**/*.h"])

cc_library(
    name = "uwebsockets",
    srcs = [],                  # no .cpp files in v20.74.0 src/
    hdrs = UWS_HDRS,
    includes = ["src"],         # so #include "uwebsockets/App.h" works

    # Compile flags mainly affect dependents that include these headers
    copts = []
    + select({":with_asio": ["-pthread"], "//conditions:default": []})
    + select({":with_asan": ["-fsanitize=address", "-g"], "//conditions:default": []})
    + select({":with_lto":  ["-flto=auto"], "//conditions:default": []})
    + select({":with_libdeflate": ["-DUWS_USE_LIBDEFLATE"], "//conditions:default": []})
    + select({":with_zlib": [], "//conditions:default": ["-DUWS_NO_ZLIB"]}),

    # Linkopts kept minimal; this target itself has no objects, but
    # we keep typical flags for consumers if Bazel propagates them.
    linkopts = []
    + select({":with_zlib": ["-lz"], "//conditions:default": []})
    + select({":with_libuv": ["-luv"], "//conditions:default": []})
    + select({":with_asio": ["-lpthread"], "//conditions:default": []})
    + select({":with_asan": ["-fsanitize=address"], "//conditions:default": []})
    + select({":with_lto":  ["-flto=auto"], "//conditions:default": []}),

    # Always depend on uSockets; its CcInfo exports uSockets headers & libs
    deps = [
        "@usockets//:uSockets",
    ]
    + select({
        ":with_libdeflate": ["@libdeflate//:libdeflate"],
        "//conditions:default": [],
    })
    + select({
        ":with_boringssl": ["@boringssl//:boringssl"],
        "//conditions:default": [],
    })
    + select({
        ":with_quic": ["@lsquic//:lsquic"],
        "//conditions:default": [],
    }),

    visibility = ["//visibility:public"],
)

alias(
    name = "uWebSockets",
    actual = ":uwebsockets",
    visibility = ["//visibility:public"],
)
""",
    )

uwebsockets = module_extension(implementation = _uwebsockets_impl)
