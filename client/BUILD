cc_library(
    name = "chat_client_lib",
    srcs = ["ChatClient.cpp"],
    hdrs = ["ChatClient.h"],
    deps = [
        ":chat_client_app_lib",
        "@nlohmann_json//:nlohmann_json",
    ],
    copts = ["-std=c++17"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "chat_client_app_lib",
    srcs = ["ChatClientApp.cpp"],
    hdrs = ["ChatClientApp.h"],
    deps = [
        "@nlohmann_json//:nlohmann_json",
        "@websocketpp//:websocketpp",
    ],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "client",
    srcs = ["client.cpp"],
    deps = [
        ":chat_client_app_lib",
        "@nlohmann_json//:nlohmann_json",
        "@websocketpp//:websocketpp",
    ],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread"],
)
