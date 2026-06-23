#include "core/PublicEndpointConfig.h"

#include "utils/EnvLoader.h"

#include <arpa/inet.h>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace core {
namespace {

constexpr std::array<std::string_view, 11> kRetiredPublicUrlVariables = {
    "CADDY_SITE_HOST",          "CADDY_ALLOWED_ORIGIN_1", "CADDY_ALLOWED_ORIGIN_2",
    "CADDY_WSS_ORIGIN_1",       "CADDY_WSS_ORIGIN_2",     "LIVEKIT_NODE1_PUBLIC_URL",
    "LIVEKIT_NODE2_PUBLIC_URL", "INVITE_BASE_URL",        "SUPABASE_PROJECT_ORIGIN",
    "SUPABASE_EXPECTED_ISS",    "ACTUAL_DOMAIN",
};

bool is_valid_port(std::string_view port) {
    if (port.empty()) return false;

    uint32_t value = 0;
    const auto [ptr, error] = std::from_chars(port.data(), port.data() + port.size(), value);
    return error == std::errc{} && ptr == port.data() + port.size() && value > 0 && value <= 65535;
}

bool is_valid_dns_or_ipv4_host(std::string_view host) {
    if (host.empty() || host.front() == '.' || host.back() == '.') return false;

    in_addr ipv4{};
    if (inet_pton(AF_INET, std::string(host).c_str(), &ipv4) == 1) return true;

    size_t label_start = 0;
    while (label_start < host.size()) {
        const auto label_end = host.find('.', label_start);
        const auto label =
            host.substr(label_start, label_end == std::string_view::npos ? host.size() - label_start
                                                                         : label_end - label_start);
        if (label.empty() || label.front() == '-' || label.back() == '-') return false;
        for (const char ch : label) {
            if (!(std::islower(static_cast<unsigned char>(ch)) ||
                  std::isdigit(static_cast<unsigned char>(ch)) || ch == '-')) {
                return false;
            }
        }
        if (label_end == std::string_view::npos) break;
        label_start = label_end + 1;
    }
    return true;
}

}  // namespace

PublicEndpointConfig PublicEndpointConfig::from_env() {
    for (const auto key : kRetiredPublicUrlVariables) {
        if (utils::EnvLoader::has_env(std::string(key))) {
            throw std::runtime_error("Retired public URL variable '" + std::string(key) +
                                     "' is set; configure only WEB_DOMAIN and "
                                     "NUXT_PUBLIC_SUPABASE_URL");
        }
    }

    return from_origins(utils::EnvLoader::get_env("WEB_DOMAIN", ""),
                        utils::EnvLoader::get_env("NUXT_PUBLIC_SUPABASE_URL", ""));
}

PublicEndpointConfig PublicEndpointConfig::from_origins(std::string app_origin,
                                                        std::string supabase_origin) {
    validate_origin("WEB_DOMAIN", app_origin);
    validate_origin("NUXT_PUBLIC_SUPABASE_URL", supabase_origin);
    return PublicEndpointConfig(std::move(app_origin), std::move(supabase_origin));
}

PublicEndpointConfig::PublicEndpointConfig(std::string app_origin, std::string supabase_origin)
    : app_origin_(std::move(app_origin)), supabase_origin_(std::move(supabase_origin)) {}

std::string PublicEndpointConfig::websocket_origin() const {
    return "wss://" + app_origin_.substr(std::string_view("https://").size());
}

std::string PublicEndpointConfig::invite_base_url() const {
    return append_path(app_origin_, "/invite");
}

std::string PublicEndpointConfig::livekit_node_url(std::string_view node_id) const {
    if (node_id.empty() || node_id.find('/') != std::string_view::npos) {
        throw std::invalid_argument("LiveKit node id must be a non-empty path segment");
    }
    return append_path(app_origin_, "/livekit/" + std::string(node_id));
}

std::string PublicEndpointConfig::livekit_cluster_url() const {
    return append_path(app_origin_, "/livekit");
}

std::string PublicEndpointConfig::supabase_issuer() const {
    return append_path(supabase_origin_, "/auth/v1");
}

void PublicEndpointConfig::validate_origin(std::string_view key, std::string_view origin) {
    constexpr std::string_view scheme = "https://";
    if (!origin.starts_with(scheme)) {
        throw std::runtime_error(std::string(key) + " must be a canonical HTTPS origin");
    }

    const auto authority = origin.substr(scheme.size());
    if (authority.empty() || authority.find_first_of("/?#@ \t\r\n") != std::string_view::npos) {
        throw std::runtime_error(std::string(key) +
                                 " must contain only an HTTPS scheme, host, and optional port");
    }
    for (const char ch : authority) {
        if (std::isupper(static_cast<unsigned char>(ch))) {
            throw std::runtime_error(std::string(key) + " must use a lowercase host");
        }
    }

    std::string_view port;
    bool has_port = false;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string_view::npos || close == 1) {
            throw std::runtime_error(std::string(key) + " contains an invalid IPv6 host");
        }
        const auto ipv6_host = authority.substr(1, close - 1);
        in6_addr ipv6{};
        if (inet_pton(AF_INET6, std::string(ipv6_host).c_str(), &ipv6) != 1) {
            throw std::runtime_error(std::string(key) + " contains an invalid IPv6 host");
        }
        const auto suffix = authority.substr(close + 1);
        if (!suffix.empty()) {
            if (!suffix.starts_with(':')) {
                throw std::runtime_error(std::string(key) + " contains an invalid authority");
            }
            port = suffix.substr(1);
            has_port = true;
        }
    } else {
        const auto first_colon = authority.find(':');
        if (first_colon == 0 || (first_colon != std::string_view::npos &&
                                 authority.find(':', first_colon + 1) != std::string_view::npos)) {
            throw std::runtime_error(std::string(key) + " contains an invalid host");
        }
        if (first_colon != std::string_view::npos) {
            port = authority.substr(first_colon + 1);
            has_port = true;
        }
        const auto host = authority.substr(0, first_colon);
        if (!is_valid_dns_or_ipv4_host(host)) {
            throw std::runtime_error(std::string(key) + " contains an invalid host");
        }
    }

    if (has_port && !is_valid_port(port)) {
        throw std::runtime_error(std::string(key) + " contains an invalid port");
    }
    if (port == "443") {
        throw std::runtime_error(std::string(key) + " must omit the default HTTPS port :443");
    }
}

std::string PublicEndpointConfig::append_path(std::string_view origin, std::string_view path) {
    if (!path.starts_with('/')) {
        throw std::invalid_argument("Public endpoint path must start with '/'");
    }
    return std::string(origin) + std::string(path);
}

}  // namespace core
