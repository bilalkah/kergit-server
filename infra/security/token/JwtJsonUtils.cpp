#include "infra/security/token/JwtJsonUtils.h"

#include <cctype>
#include <cstring>

namespace infra::security::token::jwt_scan {

namespace {

inline const char* skip_ws(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

inline bool match_key(const char* p, const char* end, const char* key, size_t len) {
    return (size_t)(end - p) >= len && std::memcmp(p, key, len) == 0;
}

inline bool parse_string(const char*& p, const char* end, std::string_view& out) {
    if (p >= end || *p != '"') return false;
    const char* start = ++p;
    while (p < end && *p != '"') ++p;
    if (p >= end) return false;
    out = std::string_view(start, p - start);
    ++p;
    return true;
}

inline bool parse_int(const char*& p, const char* end, int64_t& out) {
    bool neg = false;
    if (p < end && *p == '-') {
        neg = true;
        ++p;
    }
    if (p >= end || !std::isdigit(*p)) return false;
    int64_t v = 0;
    while (p < end && std::isdigit(*p)) {
        v = v * 10 + (*p - '0');
        ++p;
    }
    out = neg ? -v : v;
    return true;
}

}  // namespace

// ------------------------------------------------------------
// HEADER
// ------------------------------------------------------------

bool parse_header(std::string_view json, std::string_view& alg, std::string_view& kid) {
    const char* p = json.data();
    const char* end = p + json.size();

    alg = {};
    kid = {};

    while (p < end) {
        p = skip_ws(p, end);
        if (*p != '"') {
            ++p;
            continue;
        }

        std::string_view key;
        if (!parse_string(p, end, key)) return false;
        p = skip_ws(p, end);
        if (p >= end || *p != ':') return false;
        ++p;
        p = skip_ws(p, end);

        if (key == "alg") {
            if (!parse_string(p, end, alg)) return false;
        } else if (key == "kid") {
            parse_string(p, end, kid);  // optional
        } else {
            // skip value
            while (p < end && *p != ',' && *p != '}') ++p;
        }
    }

    return !alg.empty();
}

// ------------------------------------------------------------
// PAYLOAD
// ------------------------------------------------------------

bool parse_payload(std::string_view json, UserClaims& c) {
    const char* p = json.data();
    const char* end = p + json.size();

    c = {};  // reset

    while (p < end) {
        p = skip_ws(p, end);
        if (*p != '"') {
            ++p;
            continue;
        }

        std::string_view key;
        if (!parse_string(p, end, key)) return false;
        p = skip_ws(p, end);
        if (p >= end || *p != ':') return false;
        ++p;
        p = skip_ws(p, end);

        if (key == "sub") {
            std::string_view v;
            if (!parse_string(p, end, v)) return false;
            c.id.assign(v);
        } else if (key == "iss") {
            std::string_view v;
            if (!parse_string(p, end, v)) return false;
            c.iss.assign(v);
        } else if (key == "role") {
            std::string_view v;
            parse_string(p, end, v);
            c.role.assign(v);
        } else if (key == "exp") {
            if (!parse_int(p, end, c.exp)) return false;
        } else if (key == "iat") {
            parse_int(p, end, c.iat);
        } else if (key == "aud") {
            if (*p == '"') {
                std::string_view v;
                if (!parse_string(p, end, v)) return false;
                c.aud.assign(v);
            } else if (*p == '[') {
                ++p;
                p = skip_ws(p, end);
                std::string_view v;
                if (!parse_string(p, end, v)) return false;
                c.aud.assign(v);
                while (p < end && *p != ']') ++p;
                if (p < end) ++p;
            }
        } else {
            // skip unknown value
            while (p < end && *p != ',' && *p != '}') ++p;
        }
    }

    return !c.id.empty() && c.exp != 0 && !c.iss.empty() && !c.aud.empty();
}

}  // namespace infra::security::token::jwt_scan
