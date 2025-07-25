cc_binary(
    name = "server",
    srcs = ["server.cpp"],
    deps = [
        "@nlohmann_json//:nlohmann_json",
        "@uwebsockets//:uwebsockets",
    ],
    copts = ["-std=c++17"],
    linkopts = ["-lpthread", "-lz"],
)
