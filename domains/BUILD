load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "message_domain",
    hdrs = [
        "Message.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "user_domain",
    hdrs = [
        "User.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "hub_domain",
    hdrs = [
        "Hub.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "channel_domain",
    hdrs = [
        "Channel.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//domains/ids:domain_ids",
    ],
)

cc_library(
    name = "domains",
    visibility = ["//visibility:public"],
    deps = [
        ":channel_domain",
        ":hub_domain",
        ":message_domain",
        ":user_domain",
        "//domains/ids:domain_ids",
    ],
)
