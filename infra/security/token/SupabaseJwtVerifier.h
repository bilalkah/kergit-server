#ifndef INFRA_SECURITY_TOKEN_JWTVERFIER_H
#define INFRA_SECURITY_TOKEN_JWTVERFIER_H

#include "infra/security/token/ITokenVerifier.h"
#include "utils/Loggable.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

struct evp_pkey_st;

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
    std::string alg;
    std::string kid;

    std::string_view signing_input;  // header_b64 + "." + payload_b64
    std::string_view signature_b64;

    UserClaims claims;
};

/*
Big-picture flow:
- Supabase provides two JWKs via env vars: current + standby. Each JWK carries
  alg/kid and the public EC P-256 key as base64url x/y coordinates.
- On create(), we parse each JWK once and build an EVP_PKEY so verification does
  not rebuild keys in the hot path.
- JWT format is header.payload.signature (base64url). The header holds alg/kid;
  the payload holds claims like sub/iss/aud/exp. We split the token into
  string_view slices, decode header/payload, and parse only the needed fields.
- Verification picks the key by kid (current or standby), falls back to the
  other key for rotation, then verifies the ES256 signature.
- Claims validation happens after parsing: required claims must be present and
  exp is checked against current time.
*/
class SupabaseJWTVerifier : public utils::Loggable, public ITokenVerifier {
   public:
    static std::expected<SupabaseJWTVerifier, JwtVerifyError> create();

    SupabaseJWTVerifier() = default;
    ~SupabaseJWTVerifier() = default;

    JwtVerifyResult verify_token(std::string_view token) const override;

   private:
    struct KeyMaterial {
        SupabaseJWK jwk;
        std::shared_ptr<evp_pkey_st> pkey;
    };

    KeyMaterial current_key_;
    KeyMaterial standby_key_;

    static std::optional<KeyMaterial> parse_jwk(const std::string& jwk_json);
    static std::shared_ptr<evp_pkey_st> build_public_key(const SupabaseJWK& jwk);
    SupabaseJWTVerifier(KeyMaterial current, KeyMaterial standby);

    // Helper methods
    std::expected<ParsedJwt, JwtVerifyError> parse_token(std::string_view token) const;
    bool split_token(std::string_view token, std::string_view& header, std::string_view& payload,
                     std::string_view& signature, std::string_view& signing_input) const;
    bool verify_with_key(const ParsedJwt& jwt, const KeyMaterial& key) const;
    bool verify_es256_signature(std::string_view header_payload, std::string_view signature_b64,
                                const KeyMaterial& key) const;
    static bool base64_decode(std::string_view input, std::string& output);
};
}  // namespace infra::security::token

#endif  // INFRA_SECURITY_TOKEN_JWTVERFIER_H
