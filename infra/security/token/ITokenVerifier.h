#ifndef INFRA_SECURITY_TOKEN_ITOKENVERIFIER_H
#define INFRA_SECURITY_TOKEN_ITOKENVERIFIER_H

#include <chrono>
#include <cstdint>
#include <expected>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>

namespace infra::security::token {

struct UserClaims {
    std::string id;
    std::string iss;
    std::string aud;
    int64_t exp{0};
    int64_t iat{0};
    std::string role;

    // For debugging
    static std::string to_utc_string(int64_t epoch) {
        if (epoch == 0) return "";
        std::time_t t = static_cast<std::time_t>(epoch);
        std::tm tm = *std::gmtime(&t);  // convert to UTC
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S UTC");
        return oss.str();
    }

    // For debugging
    friend std::ostream& operator<<(std::ostream& os, const UserClaims& u) {
        os << "UserClaims {\n";
        if (!u.id.empty()) os << "  id: " << u.id << "\n";
        if (!u.role.empty()) os << "  role: " << u.role << "\n";
        if (!u.aud.empty()) os << "  aud: " << u.aud << "\n";
        if (!u.iss.empty()) os << "  iss: " << u.iss << "\n";
        if (u.exp != 0) os << "  exp: " << u.exp << " (" << to_utc_string(u.exp) << ")\n";
        if (u.iat != 0) os << "  iat: " << u.iat << " (" << to_utc_string(u.iat) << ")\n";
        os << "}";
        return os;
    }
};

enum class JwtVerifyError {
    EmptyToken,
    InvalidFormat,
    UnsupportedAlgorithm,
    InvalidSignature,
    TokenExpired,
    TokenNotYetValid,
    MissingClaims,
    KeyNotFound,
    JwkParseError,
    IssuerMismatch,
    AudienceMismatch,
};

using JwtVerifyResult = std::expected<UserClaims, JwtVerifyError>;

class ITokenVerifier {
   public:
    virtual ~ITokenVerifier() = default;

    virtual JwtVerifyResult verify_token(std::string_view token) const = 0;
};

}  // namespace infra::security::token

#endif  // INFRA_SECURITY_TOKEN_ITOKENVERIFIER_H
