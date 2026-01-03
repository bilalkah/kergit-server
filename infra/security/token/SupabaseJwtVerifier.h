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

struct ParsedJwt {
    std::string header_b64;
    std::string payload_b64;
    std::string signature_b64;

    std::string alg;
    std::string kid;

    std::string signing_input;  // header_b64 + "." + payload_b64

    UserClaims claims;
};

class SupabaseJWTVerifier : public utils::Loggable, public ITokenVerifier {
   public:
    static std::expected<SupabaseJWTVerifier, JwtVerifyError> create();

    ~SupabaseJWTVerifier() = default;

    JwtVerifyResult verify_token(const std::string& token) const override;

   private:
    SupabaseJWK current_key_;
    SupabaseJWK standby_key_;
    static std::optional<SupabaseJWK> parse_jwk(const std::string& jwk_json);
    SupabaseJWTVerifier(SupabaseJWK current, SupabaseJWK standby);

    // Helper methods
    std::expected<ParsedJwt, JwtVerifyError> parse_token(const std::string& token) const;
    std::vector<std::string> split_token(const std::string& token) const;
    bool verify_with_key(const ParsedJwt& jwt, const SupabaseJWK& key) const;
    bool verify_es256_signature(const std::string& header_payload, const std::string& signature_b64,
                                const SupabaseJWK& key) const;
    std::string base64_decode(const std::string& input) const;
};
}  // namespace infra::security::token

#endif  // INFRA_SECURITY_TOKEN_JWTVERFIER_H
