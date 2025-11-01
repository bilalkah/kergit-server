#ifndef INFRA_SECURITY_TOKEN_JWTVERFIER_H
#define INFRA_SECURITY_TOKEN_JWTVERFIER_H

#include "infra/security/token/ITokenVerifier.h"
#include "utils/Loggable.h"

#include <optional>
#include <vector>

namespace infra::security::token {

struct SupabaseJWK {
    std::string kty;  // key type
    std::string alg;  // algorithm
    std::string kid;  // key id
    std::string x;    // x coordinate (for EC keys)
    std::string y;    // y coordinate (for EC keys)
    std::string crv;  // curve (for EC keys)
    std::string n;    // modulus (for RSA keys)
    std::string e;    // exponent (for RSA keys)
};

class SupabaseJWTVerifier : public utils::Loggable {
   public:
    SupabaseJWTVerifier();
    ~SupabaseJWTVerifier();

    // Verify JWT token and extract user information
    std::optional<UserClaims> verify_token(const std::string& token);

    // Check if token is valid (not expired, properly signed)
    bool is_token_valid(const std::string& token);

    // Check if token is expired
    bool is_token_expired(const std::string& token);

   private:
    SupabaseJWK current_key_;
    SupabaseJWK standby_key_;

    // Helper methods
    std::optional<UserClaims> decode_and_verify(const std::string& token, const SupabaseJWK& key);
    std::string base64_decode(const std::string& input);
    bool verify_es256_signature(const std::string& header_payload, const std::string& signature_b64,
                                const SupabaseJWK& key);
    std::vector<std::string> split_token(const std::string& token);
    int64_t get_current_timestamp();
    SupabaseJWK parse_jwk(const std::string& jwk_json);
};
}  // namespace infra::security::token

#endif  // INFRA_SECURITY_TOKEN_JWTVERFIER_H
