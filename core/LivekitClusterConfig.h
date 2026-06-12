#ifndef CORE_LIVEKITCLUSTERCONFIG_H
#define CORE_LIVEKITCLUSTERCONFIG_H

#include "core/PublicEndpointConfig.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace core {

struct LivekitNodeConfig {
    std::string id;
    uint16_t signal_port;
    uint16_t rtc_tcp_port;
    uint16_t rtc_udp_start;
    uint16_t rtc_udp_end;
    uint16_t prometheus_port;
    std::string node_ip;

    std::string private_url;
    std::string public_url;
    std::string caddy_target;
    std::string metrics_target;
};

class LivekitClusterConfig {
   public:
    static LivekitClusterConfig from_env(const PublicEndpointConfig& endpoints);
    static LivekitClusterConfig from_json(std::string_view raw,
                                          const PublicEndpointConfig& endpoints,
                                          bool require_node_ip = false);

    const std::vector<LivekitNodeConfig>& nodes() const { return nodes_; }

   private:
    explicit LivekitClusterConfig(std::vector<LivekitNodeConfig> nodes);

    std::vector<LivekitNodeConfig> nodes_;
};

}  // namespace core

#endif  // CORE_LIVEKITCLUSTERCONFIG_H
