cc_binary(
    name = "client",
    srcs = ["client.cpp"],
    deps = [
        "@nlohmann_json//:nlohmann_json",
        "@websocketpp//:websocketpp",
    ],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread"],
)
