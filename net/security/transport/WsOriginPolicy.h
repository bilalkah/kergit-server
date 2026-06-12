#ifndef NET_SECURITY_TRANSPORT_WSORIGINPOLICY_H
#define NET_SECURITY_TRANSPORT_WSORIGINPOLICY_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace net::security::transport {

class WsOriginPolicy {
   public:
    static WsOriginPolicy from_file(const std::string& path, std::string_view allowed_origin);

    bool is_allowed(std::string_view origin) const;
    bool is_trusted_proxy(std::string_view remote_ip) const;

   private:
    struct ParsedIp {
        int family{0};
        std::array<uint8_t, 16> bytes{};
    };

    struct CidrRange {
        int family{0};
        std::array<uint8_t, 16> network{};
        uint8_t prefix{0};
    };

    static std::optional<std::string> normalize_origin(std::string_view origin);
    static std::optional<ParsedIp> parse_ip_literal(std::string ip_text);
    static std::optional<CidrRange> parse_cidr(std::string_view cidr_text);
    static bool ip_matches_cidr(const ParsedIp& ip, const CidrRange& cidr);

    static std::string trim_copy(std::string_view value);
    static std::string to_lower_ascii(std::string_view value);
    static bool is_digits(std::string_view value);
    static std::string strip_quotes(std::string_view value);

    std::unordered_set<std::string> allowed_origins_{};
    std::vector<CidrRange> trusted_proxy_cidrs_{};
};

}  // namespace net::security::transport

#endif  // NET_SECURITY_TRANSPORT_WSORIGINPOLICY_H
