load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "i_command",
    hdrs = [
        "ICommand.h",
        "utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/queue:event_msg",
        "//domains:channel_domain",
        "//domains:hub_domain",
        "//domains:message_domain",
        "//domains:user_domain",
        "//domains/ids:domain_ids",
        "//net/outbound:outbound_msg",
        "//proto:envelope_cc_proto",
        "//proto/command:message_cc_proto",
        "//proto/domain:channel_cc_proto",
        "//proto/domain:hub_cc_proto",
        "//proto/domain:message_cc_proto",
        "//proto/domain:user_cc_proto",
        "//proto/event:activity_cc_proto",
        "//proto/event:error_cc_proto",
        "//proto/event:message_cc_proto",
        "//utils:metrics",
    ],
)
