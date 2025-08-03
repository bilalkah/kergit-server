cc_library(
    name = "security_basic",
    srcs = [
        "Authentication.cpp",
    ],
    hdrs = [
        "Authentication.h",
    ],
    # Note: OpenSSL dependency would be added here in production
    # For now, we'll use simplified hashing
    visibility = ["//visibility:public"],
    deps = [
        "//common",
        "@nlohmann_json",
    ],
)
