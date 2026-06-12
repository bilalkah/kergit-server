#include "core/LivekitClusterConfig.h"

#include "utils/EnvLoader.h"

#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cctype>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace core {
namespace {

using json = nlohmann::json;

constexpr std::array<std::string_view, 12> kRetiredLivekitNodeVariables = {
    "LIVEKIT_NODE1_PRIVATE_URL",
    "LIVEKIT_NODE2_PRIVATE_URL",
    "LIVEKIT_PROMETHEUS_PORT",
    "LIVEKIT_PROMETHEUS_NODE1_PORT",
    "LIVEKIT_PROMETHEUS_NODE2_PORT",
    "CADDY_LIVEKIT_NODE1_TARGET",
    "CADDY_LIVEKIT_NODE2_TARGET",
    "ADMIN_LIVEKIT_NODE1_METRICS_TARGET",
    "ADMIN_LIVEKIT_NODE2_METRICS_TARGET",
    "LIVEKIT_NODE1_CONFIG_PATH",
    "LIVEKIT_NODE2_CONFIG_PATH",
    "LIVEKIT_PORT",
};

constexpr std::array<std::string_view, 7> kNodeFields = {
    "id",          "signal_port",     "rtc_tcp_port", "rtc_udp_start",
    "rtc_udp_end", "prometheus_port", "node_ip",
};

constexpr std::array<std::string_view, 7> kReservedNodeIds = {
    "admin-node", "caddy-node",   "redis-node",    "server-node",
    "web-node",   "web-node-dev", "web-node-prod",
};

constexpr std::array<uint16_t, 8> kReservedHostTcpPorts = {
    80, 443, 3000, 3001, 6379, 8080, 8081, 9001,
};

bool is_safe_node_id(std::string_view id) {
    if (id.empty() || id.size() > 63 || !std::isalnum(static_cast<unsigned char>(id.front())) ||
        !std::isalnum(static_cast<unsigned char>(id.back()))) {
        return false;
    }
    for (const char ch : id) {
        if (!(std::islower(static_cast<unsigned char>(ch)) ||
              std::isdigit(static_cast<unsigned char>(ch)) || ch == '-')) {
            return false;
        }
    }
    return true;
}

bool is_valid_ip(std::string_view value) {
    in_addr ipv4{};
    in6_addr ipv6{};
    const std::string ip(value);
    return inet_pton(AF_INET, ip.c_str(), &ipv4) == 1 ||
           inet_pton(AF_INET6, ip.c_str(), &ipv6) == 1;
}

uint16_t parse_port(const json& node, std::string_view field) {
    const auto key = std::string(field);
    if (!node.contains(key) || !node.at(key).is_number_integer()) {
        throw std::runtime_error("LIVEKIT_NODES field '" + key + "' must be an integer");
    }
    const auto value = node.at(key).get<int64_t>();
    if (value < 1 || value > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("LIVEKIT_NODES field '" + key + "' must be between 1 and 65535");
    }
    return static_cast<uint16_t>(value);
}

void reject_unknown_fields(const json& node) {
    for (const auto& [key, _] : node.items()) {
        bool known = false;
        for (const auto field : kNodeFields) {
            if (key == field) {
                known = true;
                break;
            }
        }
        if (!known) {
            throw std::runtime_error("LIVEKIT_NODES contains unknown field '" + key + "'");
        }
    }
}

void add_tcp_port(std::unordered_set<uint16_t>& ports, uint16_t port) {
    if (std::find(kReservedHostTcpPorts.begin(), kReservedHostTcpPorts.end(), port) !=
        kReservedHostTcpPorts.end()) {
        throw std::runtime_error("LIVEKIT_NODES TCP port collides with a shared service: " +
                                 std::to_string(port));
    }
    if (!ports.insert(port).second) {
        throw std::runtime_error("LIVEKIT_NODES contains a duplicate TCP port: " +
                                 std::to_string(port));
    }
}

}  // namespace

LivekitClusterConfig LivekitClusterConfig::from_env(const PublicEndpointConfig& endpoints) {
    for (const auto key : kRetiredLivekitNodeVariables) {
        if (utils::EnvLoader::has_env(std::string(key))) {
            throw std::runtime_error("Retired LiveKit node variable '" + std::string(key) +
                                     "' is set; configure only LIVEKIT_NODES");
        }
    }
    return from_json(utils::EnvLoader::get_env("LIVEKIT_NODES", ""), endpoints,
                     utils::EnvLoader::get<bool>("LIVEKIT_PRODUCTION_MODE", false));
}

LivekitClusterConfig LivekitClusterConfig::from_json(std::string_view raw,
                                                     const PublicEndpointConfig& endpoints,
                                                     bool require_node_ip) {
    const auto document = json::parse(raw, nullptr, false);
    if (document.is_discarded() || !document.is_array() || document.empty()) {
        throw std::runtime_error("LIVEKIT_NODES must be a non-empty JSON array");
    }

    std::vector<LivekitNodeConfig> nodes;
    nodes.reserve(document.size());
    std::unordered_set<std::string> ids;
    std::unordered_set<uint16_t> tcp_ports;

    for (const auto& value : document) {
        if (!value.is_object()) {
            throw std::runtime_error("Every LIVEKIT_NODES entry must be an object");
        }
        reject_unknown_fields(value);

        if (!value.contains("id") || !value.at("id").is_string()) {
            throw std::runtime_error("LIVEKIT_NODES field 'id' must be a string");
        }
        const auto id = value.at("id").get<std::string>();
        if (!is_safe_node_id(id)) {
            throw std::runtime_error(
                "LIVEKIT_NODES node id must use lowercase letters, digits, and internal hyphens");
        }
        if (std::find(kReservedNodeIds.begin(), kReservedNodeIds.end(), id) !=
            kReservedNodeIds.end()) {
            throw std::runtime_error("LIVEKIT_NODES node id collides with a shared service");
        }
        if (ids.contains(id)) {
            throw std::runtime_error("LIVEKIT_NODES contains duplicate node id '" + id + "'");
        }
        for (const auto& existing : ids) {
            if (id.starts_with(existing) || existing.starts_with(id)) {
                throw std::runtime_error("LIVEKIT_NODES node ids must not be path prefixes");
            }
        }
        ids.insert(id);

        const auto signal_port = parse_port(value, "signal_port");
        const auto rtc_tcp_port = parse_port(value, "rtc_tcp_port");
        const auto rtc_udp_start = parse_port(value, "rtc_udp_start");
        const auto rtc_udp_end = parse_port(value, "rtc_udp_end");
        const auto prometheus_port = parse_port(value, "prometheus_port");
        if (rtc_udp_start > rtc_udp_end) {
            throw std::runtime_error("LIVEKIT_NODES rtc_udp_start must not exceed rtc_udp_end");
        }

        add_tcp_port(tcp_ports, signal_port);
        add_tcp_port(tcp_ports, rtc_tcp_port);
        add_tcp_port(tcp_ports, prometheus_port);

        std::string node_ip;
        if (value.contains("node_ip")) {
            if (!value.at("node_ip").is_string()) {
                throw std::runtime_error("LIVEKIT_NODES field 'node_ip' must be a string");
            }
            node_ip = value.at("node_ip").get<std::string>();
            if (!node_ip.empty() && !is_valid_ip(node_ip)) {
                throw std::runtime_error(
                    "LIVEKIT_NODES field 'node_ip' must be a valid IP address");
            }
        }
        if (require_node_ip && node_ip.empty()) {
            throw std::runtime_error("Production LIVEKIT_NODES entries require node_ip");
        }

        for (const auto& existing : nodes) {
            if (rtc_udp_start <= existing.rtc_udp_end && existing.rtc_udp_start <= rtc_udp_end) {
                throw std::runtime_error("LIVEKIT_NODES contains overlapping RTC UDP ranges");
            }
        }

        nodes.push_back({
            .id = id,
            .signal_port = signal_port,
            .rtc_tcp_port = rtc_tcp_port,
            .rtc_udp_start = rtc_udp_start,
            .rtc_udp_end = rtc_udp_end,
            .prometheus_port = prometheus_port,
            .node_ip = std::move(node_ip),
            .private_url = "http://" + id + ":" + std::to_string(signal_port),
            .public_url = endpoints.livekit_node_url(id),
            .caddy_target = id + ":" + std::to_string(signal_port),
            .metrics_target = id + ":" + std::to_string(prometheus_port),
        });
    }

    return LivekitClusterConfig(std::move(nodes));
}

LivekitClusterConfig::LivekitClusterConfig(std::vector<LivekitNodeConfig> nodes)
    : nodes_(std::move(nodes)) {}

}  // namespace core
