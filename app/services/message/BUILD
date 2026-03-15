load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "async_message_writer",
    srcs = ["AsyncMessageWriter.cpp"],
    hdrs = [
        "AsyncMessageWriter.h",
        "DbWriteQueue.h",
    ],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        "//core/base:ts_queue",
        "//domains:message_domain",
        "//infra/persistence:persistence_gateway_lib",
        "//utils:loggable",
        "//utils:metrics",
    ],
)

cc_library(
    name = "message_service",
    srcs = ["MessageService.cpp"],
    hdrs = ["MessageService.h"],
    copts = ["-std=gnu++23"],
    visibility = ["//app:__subpackages__"],
    deps = [
        ":async_message_writer",
        "//domains:message_domain",
        "//infra/persistence:persistence_gateway_lib",
        "//utils:loggable",
        "//utils:uuid",
    ],
)
