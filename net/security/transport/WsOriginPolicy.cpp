#include "net/security/transport/WsOriginPolicy.h"

#include <arpa/inet.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

void push_unique(std::vector<std::string>& out, const std::string& candidate) {
    if (candidate.empty()) return;
    for (const auto& existing : out) {
        if (existing == candidate) return;
    }
    out.push_back(candidate);
}

void append_runfiles_candidates(std::vector<std::string>& candidates,
                                const std::string& requested_path) {
    const std::filesystem::path rel(requested_path);
    if (rel.is_absolute()) return;

    const char* runfiles_dir = std::getenv("RUNFILES_DIR");
    if (runfiles_dir != nullptr && runfiles_dir[0] != '\0') {
        const std::filesystem::path base(runfiles_dir);
        push_unique(candidates, (base / requested_path).string());
        push_unique(candidates, (base / "_main" / requested_path).string());
    }

    const char* test_srcdir = std::getenv("TEST_SRCDIR");
    if (test_srcdir != nullptr && test_srcdir[0] != '\0') {
        const std::filesystem::path base(test_srcdir);
        const char* test_workspace = std::getenv("TEST_WORKSPACE");
        if (test_workspace != nullptr && test_workspace[0] != '\0') {
            push_unique(candidates, (base / test_workspace / requested_path).string());
        }
        push_unique(candidates, (base / "_main" / requested_path).string());
    }
}

std::string resolve_policy_path(const std::string& requested_path) {
    std::vector<std::string> candidates{};
    push_unique(candidates, requested_path);

    const std::filesystem::path rel(requested_path);
    if (!rel.is_absolute()) {
        const char* workspace_dir = std::getenv("BUILD_WORKSPACE_DIRECTORY");
        if (workspace_dir != nullptr && workspace_dir[0] != '\0') {
            push_unique(candidates, (std::filesystem::path(workspace_dir) / rel).string());
        }
    }
    append_runfiles_candidates(candidates, requested_path);

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    std::string checked{};
    for (size_t i = 0; i < candidates.size(); ++i) {
        checked += candidates[i];
        if (i + 1 < candidates.size()) checked += ", ";
    }
    throw std::runtime_error("WS origin policy file missing: " + requested_path +
                             " (checked: " + checked + ")");
}

}  // namespace

namespace net::security::transport {

WsOriginPolicy WsOriginPolicy::from_file(const std::string& path) {
    const std::string resolved_path = resolve_policy_path(path);
    std::ifstream in(resolved_path);
    if (!in.is_open()) {
        throw std::runtime_error("WS origin policy file missing: " + resolved_path);
    }

    WsOriginPolicy policy{};

    enum class Section : uint8_t {
        None = 0,
        AllowedOrigins = 1,
        TrustedProxyCidrs = 2,
    };

    Section section = Section::None;
    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;

        const auto hash_pos = line.find('#');
        if (hash_pos != std::string::npos) {
            line = line.substr(0, hash_pos);
        }

        std::string entry = trim_copy(line);
        if (entry.empty()) continue;

        if (entry == "allowed_origins:") {
            section = Section::AllowedOrigins;
            continue;
        }
        if (entry == "trusted_proxy_cidrs:") {
            section = Section::TrustedProxyCidrs;
            continue;
        }

        if (entry.front() != '-') {
            throw std::runtime_error("Invalid WS origin policy line " + std::to_string(line_no) +
                                     ": unsupported entry");
        }
        if (section == Section::None) {
            throw std::runtime_error("Invalid WS origin policy line " + std::to_string(line_no) +
                                     ": list entry before section");
        }

        std::string value = trim_copy(entry.substr(1));
        value = strip_quotes(value);
        if (value.empty()) {
            throw std::runtime_error("Invalid WS origin policy line " + std::to_string(line_no) +
                                     ": empty value");
        }

        if (section == Section::AllowedOrigins) {
            auto normalized = normalize_origin(value);
            if (!normalized.has_value()) {
                throw std::runtime_error("Invalid allowed origin at line " +
                                         std::to_string(line_no) + ": " + value);
            }
            policy.allowed_origins_.insert(std::move(normalized.value()));
            continue;
        }

        auto cidr = parse_cidr(value);
        if (!cidr.has_value()) {
            throw std::runtime_error("Invalid trusted proxy CIDR at line " +
                                     std::to_string(line_no) + ": " + value);
        }
        policy.trusted_proxy_cidrs_.push_back(cidr.value());
    }

    if (policy.allowed_origins_.empty()) {
        throw std::runtime_error("WS origin policy invalid: allowed_origins is empty");
    }
    if (policy.trusted_proxy_cidrs_.empty()) {
        throw std::runtime_error("WS origin policy invalid: trusted_proxy_cidrs is empty");
    }

    return policy;
}

bool WsOriginPolicy::is_allowed(std::string_view origin) const {
    auto normalized = normalize_origin(origin);
    if (!normalized.has_value()) return false;
    return allowed_origins_.find(normalized.value()) != allowed_origins_.end();
}

bool WsOriginPolicy::is_trusted_proxy(std::string_view remote_ip) const {
    auto parsed = parse_ip_literal(std::string(remote_ip));
    if (!parsed.has_value()) return false;
    for (const auto& cidr : trusted_proxy_cidrs_) {
        if (ip_matches_cidr(parsed.value(), cidr)) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> WsOriginPolicy::normalize_origin(std::string_view origin) {
    std::string value = trim_copy(origin);
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    if (value.empty()) return std::nullopt;

    const auto scheme_pos = value.find("://");
    if (scheme_pos == std::string::npos) return std::nullopt;

    std::string scheme = to_lower_ascii(std::string_view(value).substr(0, scheme_pos));
    if (scheme != "http" && scheme != "https") return std::nullopt;

    std::string authority_and_path = value.substr(scheme_pos + 3);
    if (authority_and_path.empty()) return std::nullopt;

    std::string authority = authority_and_path;
    const auto slash_pos = authority_and_path.find('/');
    if (slash_pos != std::string::npos) {
        authority = authority_and_path.substr(0, slash_pos);
    }
    if (authority.empty()) return std::nullopt;
    if (authority.find('@') != std::string::npos) return std::nullopt;

    std::string host;
    std::string port;
    if (authority.front() == '[') {
        const auto close_bracket = authority.find(']');
        if (close_bracket == std::string::npos) return std::nullopt;
        host = authority.substr(1, close_bracket - 1);
        const auto rem = authority.substr(close_bracket + 1);
        if (!rem.empty()) {
            if (rem.front() != ':' || !is_digits(std::string_view(rem).substr(1))) {
                return std::nullopt;
            }
            port = rem;
        }
        host = "[" + to_lower_ascii(host) + "]";
    } else {
        const auto first_colon = authority.find(':');
        const auto last_colon = authority.rfind(':');
        if (first_colon != std::string::npos && first_colon != last_colon) {
            return std::nullopt;  // IPv6 host must be bracketed in Origin.
        }

        if (first_colon == std::string::npos) {
            host = authority;
        } else {
            host = authority.substr(0, first_colon);
            const auto raw_port = authority.substr(first_colon + 1);
            if (!is_digits(raw_port)) return std::nullopt;
            port = ":" + raw_port;
        }
        host = to_lower_ascii(host);
    }

    if (host.empty()) return std::nullopt;

    return scheme + "://" + host + port;
}

std::optional<WsOriginPolicy::ParsedIp> WsOriginPolicy::parse_ip_literal(std::string ip_text) {
    ip_text = trim_copy(ip_text);
    if (ip_text.empty()) return std::nullopt;

    if (ip_text.front() == '[') {
        auto end_bracket = ip_text.find(']');
        if (end_bracket == std::string::npos) return std::nullopt;
        ip_text = ip_text.substr(1, end_bracket - 1);
    } else {
        const auto colon = ip_text.rfind(':');
        if (colon != std::string::npos && ip_text.find(':') == colon &&
            ip_text.find('.') != std::string::npos && is_digits(ip_text.substr(colon + 1))) {
            ip_text = ip_text.substr(0, colon);
        }
    }

    ParsedIp parsed{};
    in_addr addr4{};
    if (::inet_pton(AF_INET, ip_text.c_str(), &addr4) == 1) {
        parsed.family = AF_INET;
        std::memcpy(parsed.bytes.data(), &addr4, sizeof(addr4));
        return parsed;
    }

    in6_addr addr6{};
    if (::inet_pton(AF_INET6, ip_text.c_str(), &addr6) == 1) {
        if (IN6_IS_ADDR_V4MAPPED(&addr6)) {
            parsed.family = AF_INET;
            std::memcpy(parsed.bytes.data(), addr6.s6_addr + 12, sizeof(in_addr));
            return parsed;
        }
        parsed.family = AF_INET6;
        std::memcpy(parsed.bytes.data(), &addr6, sizeof(addr6));
        return parsed;
    }

    return std::nullopt;
}

std::optional<WsOriginPolicy::CidrRange> WsOriginPolicy::parse_cidr(std::string_view cidr_text) {
    std::string raw = trim_copy(cidr_text);
    if (raw.empty()) return std::nullopt;

    std::string ip_part = raw;
    int prefix = -1;
    const auto slash = raw.find('/');
    if (slash != std::string::npos) {
        ip_part = raw.substr(0, slash);
        const auto prefix_part = raw.substr(slash + 1);
        if (!is_digits(prefix_part)) return std::nullopt;
        prefix = std::stoi(prefix_part);
    }

    auto ip = parse_ip_literal(ip_part);
    if (!ip.has_value()) return std::nullopt;

    const int max_prefix = ip->family == AF_INET ? 32 : 128;
    if (prefix < 0) {
        prefix = max_prefix;
    }
    if (prefix < 0 || prefix > max_prefix) return std::nullopt;

    CidrRange out{};
    out.family = ip->family;
    out.network = ip->bytes;
    out.prefix = static_cast<uint8_t>(prefix);
    return out;
}

bool WsOriginPolicy::ip_matches_cidr(const ParsedIp& ip, const CidrRange& cidr) {
    if (ip.family != cidr.family) return false;
    const size_t total_bytes = ip.family == AF_INET ? 4u : 16u;
    const size_t full_bytes = cidr.prefix / 8;
    const uint8_t rem_bits = static_cast<uint8_t>(cidr.prefix % 8);

    for (size_t i = 0; i < full_bytes && i < total_bytes; ++i) {
        if (ip.bytes[i] != cidr.network[i]) return false;
    }

    if (rem_bits != 0 && full_bytes < total_bytes) {
        const uint8_t mask = static_cast<uint8_t>(0xFFu << (8u - rem_bits));
        if ((ip.bytes[full_bytes] & mask) != (cidr.network[full_bytes] & mask)) return false;
    }

    return true;
}

std::string WsOriginPolicy::trim_copy(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return std::string(value.substr(start, end - start));
}

std::string WsOriginPolicy::to_lower_ascii(std::string_view value) {
    std::string out(value);
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool WsOriginPolicy::is_digits(std::string_view value) {
    if (value.empty()) return false;
    for (char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    }
    return true;
}

std::string WsOriginPolicy::strip_quotes(std::string_view value) {
    std::string out = trim_copy(value);
    if (out.size() >= 2 &&
        ((out.front() == '"' && out.back() == '"') || (out.front() == '\'' && out.back() == '\''))) {
        out = out.substr(1, out.size() - 2);
    }
    return trim_copy(out);
}

}  // namespace net::security::transport
