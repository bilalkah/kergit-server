"""WebRTC library extension for Bazel."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def _webrtc_impl(ctx):
    """Implementation of the WebRTC extension."""
    
    # For now, let's use a simpler approach with a WebRTC wrapper
    # We'll start with a basic implementation and can upgrade later
    maybe(
        git_repository,
        name = "webrtc_simple",
        remote = "https://github.com/webrtc/webrtc.git",
        commit = "branch-heads/5060",  # Use a stable branch
        shallow_since = "2023-01-01",
    )

webrtc = module_extension(
    implementation = _webrtc_impl,
) 