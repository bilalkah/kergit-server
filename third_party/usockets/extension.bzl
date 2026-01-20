""" Bazel extension for uSockets."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party:versions.bzl", "USOCKETS_SHA256", "USOCKETS_VERSION")

def _usockets_impl(ctx):
    http_archive(
        name = "usockets",
        url = "https://github.com/uNetworking/uSockets/archive/refs/tags/v{}.tar.gz".format(USOCKETS_VERSION),
        strip_prefix = "uSockets-{}".format(USOCKETS_VERSION),
        sha256 = "{}".format(USOCKETS_SHA256),
        build_file_content = """
load("@rules_cc//cc:cc_library.bzl", "cc_library")

# ===== Feature switches via --define =====
config_setting(name = "with_boringssl", define_values = {"WITH_BORINGSSL": "1"})
config_setting(name = "with_openssl",  define_values = {"WITH_OPENSSL": "1"})
config_setting(name = "with_wolfssl",  define_values = {"WITH_WOLFSSL": "1"})
config_setting(name = "with_io_uring", define_values = {"WITH_IO_URING": "1"})
config_setting(name = "with_libuv",    define_values = {"WITH_LIBUV": "1"})
config_setting(name = "with_asio",     define_values = {"WITH_ASIO": "1"})
config_setting(name = "with_gcd",      define_values = {"WITH_GCD": "1"})  # macOS
config_setting(name = "with_asan",     define_values = {"WITH_ASAN": "1"})
config_setting(name = "with_quic",     define_values = {"WITH_QUIC": "1"})
config_setting(name = "with_lto",      define_values = {"WITH_LTO": "1"})

# ===== Sources from repo =====
USOCKETS_C_SRCS = glob([
    "src/*.c",
    "src/eventing/*.c",
    "src/crypto/*.c",
    "src/io_uring/*.c",
])

USOCKETS_SSL_CPP = glob(["src/crypto/*.cpp"])
USOCKETS_ASIO_CPP = ["src/eventing/asio.cpp"]

cc_library(
    name = "uSockets",
    srcs = USOCKETS_C_SRCS
        + select({":with_asio": USOCKETS_ASIO_CPP, "//conditions:default": []})
        + select({
            ":with_boringssl": USOCKETS_SSL_CPP,
            ":with_openssl":  USOCKETS_SSL_CPP,
            "//conditions:default": [],
        }),
    hdrs = glob(["src/**/*.h"]),
    includes = ["src"],

    copts = []
    + select({":with_boringssl": ["-DLIBUS_USE_OPENSSL", "-pthread"],
              ":with_openssl":  ["-DLIBUS_USE_OPENSSL"],
              ":with_wolfssl":  ["-DLIBUS_USE_WOLFSSL"],
              "//conditions:default": ["-DLIBUS_NO_SSL"]})
    + select({":with_io_uring": ["-DLIBUS_USE_IO_URING"], "//conditions:default": []})
    + select({":with_libuv": ["-DLIBUS_USE_LIBUV", "-I/usr/include", "-I/usr/local/include"], "//conditions:default": []})
    + select({":with_asio": ["-DLIBUS_USE_ASIO"], "//conditions:default": []})
    + select({":with_gcd": ["-DLIBUS_USE_GCD"], "//conditions:default": []})
    + select({":with_asan": ["-fsanitize=address", "-g"], "//conditions:default": []})
    + select({":with_lto": ["-flto"], "//conditions:default": []}),

    cxxopts = select({
        ":with_asio": ["-std=c++17", "-pthread"],
        ":with_boringssl": ["-std=c++17"],
        ":with_openssl":  ["-std=c++17"],
        "//conditions:default": [],
    })
    + select({":with_asan": ["-fsanitize=address"], "//conditions:default": []})
    + select({":with_lto": ["-flto"], "//conditions:default": []}),

    linkopts = []
    + select({":with_boringssl": ["-pthread", "-lstdc++"],
              ":with_openssl":  ["-lssl", "-lcrypto", "-lstdc++"],
              ":with_wolfssl":  ["-L/usr/local/lib", "-lwolfssl"],
              "//conditions:default": []})
    + select({":with_libuv": ["-luv"], "//conditions:default": []})
    + select({":with_gcd": ["-framework", "CoreFoundation"], "//conditions:default": []})
    + select({":with_io_uring": ["-luring"], "//conditions:default": []})
    + select({":with_asan": ["-fsanitize=address"], "//conditions:default": []})
    + select({":with_lto": ["-flto"], "//conditions:default": []}),

    deps = []
    + select({":with_boringssl": ["@boringssl//:boringssl"], "//conditions:default": []})
    + select({":with_quic": ["@lsquic//:lsquic"], "//conditions:default": []}),

    visibility = ["//visibility:public"],
)

# Convenience alias to mimic Makefile output name
alias(name = "uSockets.a", actual = ":uSockets", visibility = ["//visibility:public"])
""",
    )

usockets = module_extension(
    implementation = _usockets_impl,
)
