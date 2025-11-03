# dummy file
load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")

# Export files for Docker build
exports_files([
    "requirements.txt",
    ".env",
])

refresh_compile_commands(
    name = "refresh_compile_commands",

    # Specify the targets of interest.
    # For example, specify a dict of targets and any flags required to build.
    targets = {
        "//app:server_app": "--config=sslquic_dbg",
    },
    # No need to add flags already in .bazelrc. They're automatically picked up.
    # If you don't need flags, a list of targets is also okay, as is a single target string.
    # Wildcard patterns, like //... for everything, *are* allowed here, just like a build.
    # As are additional targets (+) and subtractions (-), like in bazel query https://docs.bazel.build/versions/main/query.html#expressions
    # And if you're working on a header-only library, specify a test or binary target that compiles it.
)

sh_binary(
    name = "format",
    srcs = ["scripts/format.sh"],
    data = [
        ".clang-format",
    ],
)

sh_binary(
    name = "list_format_files",
    srcs = ["scripts/list_files.sh"],
    data = [
        ".clang-format",
    ],
)
