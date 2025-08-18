"""Centralized version management for third-party dependencies."""

# libpqxx versions
LIBPQXX_VERSION = "7.10.1"
LIBPQXX_SHA256 = "cfbbb1d93a0a3d81319ec71d9a3db80447bb033c4f6cee088554a88862fd77d7"

# nlohmann_json versions
NLOHMANN_JSON_VERSION = "v3.12.0"
NLOHMANN_JSON_SHA256 = "42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa"

# uSockets versions
USOCKETS_VERSION = "v0.8.8"
USOCKETS_SHA256 = "d14d2efe1df767dbebfb8d6f5b52aa952faf66b30c822fbe464debaa0c5c0b17"

# BoringSSL versions
BORINGSSL_VERSION = "0.20250807.0"
BORINGSSL_SHA256 = "0cd3a2ba242ead4b3365b8c66cfed4a5f3f0ae511e9c0fd627edee1252d3dbe2"

# LSQUIC versions
LSQUIC_VERSION = "v4.0.12"
LSQUIC_SHA256 = "9dfbb5617059f6085c3d796dae3850c9e8a65f2e35582af12babeed633a22be7"

# uWebSockets versions
UWEBSOCKETS_VERSION = "v20.62.0"
UWEBSOCKETS_SHA256 = "8c6c6d8b9247c33c0f7c4768c67b9e5c3c8c3c8c3c8c3c8c3c8c3c8c3c8c3c8c"

# # Version compatibility matrix
# VERSION_COMPATIBILITY = {
#     "usockets": {
#         "boringssl": ["boringssl-20231212", "boringssl-20231115"],
#         "lsquic": ["v4.0.7", "v4.0.6"],
#         "openssl": ["1.1.1", "3.0.0", "3.1.0", "3.2.0"],
#     },
#     "uwebsockets": {
#         "usockets": ["v0.8.8", "v0.8.7"],
#     },
# }

# # Build configuration presets
# BUILD_PRESETS = {
#     "production": {
#         "usockets_ssl": "openssl",  # Use system OpenSSL for production
#         "usockets_quic": "disabled",  # Disable QUIC for production
#         "usockets_io_uring": "disabled",  # Disable io_uring for production
#     },
#     "development": {
#         "usockets_ssl": "boringssl",  # Use BoringSSL for development
#         "usockets_quic": "enabled",  # Enable QUIC for development
#         "usockets_io_uring": "enabled",  # Enable io_uring for development
#     },
#     "minimal": {
#         "usockets_ssl": "openssl",  # Use system OpenSSL
#         "usockets_quic": "disabled",  # Disable QUIC
#         "usockets_io_uring": "disabled",  # Disable io_uring
#     },
# } 