load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "audit_service",
    srcs = ["AuditService.cpp"],
    hdrs = [
        "AuditService.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//core/base:ts_queue",
        "//infra/persistence:persistence_gateway_lib",
        "//utils:loggable",
    ],
)
