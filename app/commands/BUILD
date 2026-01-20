load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "i_command",
    hdrs = [
        "ICommand.h",
        "utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//app/queue:event_msg",
        "//domains/ids:domain_ids",
        "//net/outbound:outbound_msg",
        "//proto:envelope_cc_proto",
        "//proto/command:message_cc_proto",
        "//proto/event:error_cc_proto",
    ],
)
