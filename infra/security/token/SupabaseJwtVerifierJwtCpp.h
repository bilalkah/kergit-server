#ifndef INFRA_SECURITY_TOKEN_SUPABASE_JWT_VERIFIER_JWTCPP_H
#define INFRA_SECURITY_TOKEN_SUPABASE_JWT_VERIFIER_JWTCPP_H

#include "infra/security/token/ITokenVerifier.h"
#include "infra/security/token/SupabaseJwtVerifier.h"
#include "utils/Loggable.h"

#include <expected>
#include <jwt-cpp/jwt.h>
#include <optional>
#include <string>
#include <string_view>

namespace infra::security::token {

class SupabaseJWTVerifierJwtCpp : public utils::Loggable, public ITokenVerifier {
   public:
    static std::expected<SupabaseJWTVerifierJwtCpp, JwtVerifyError> create();

    ~SupabaseJWTVerifierJwtCpp() = default;

    JwtVerifyResult verify_token(std::string_view token) const override;

   private:
    struct KeyMaterial {
        SupabaseJWK jwk;
        jwt::algorithm::es256 verifier;
    };

    KeyMaterial current_key_;
    KeyMaterial standby_key_;

    static std::optional<KeyMaterial> parse_jwk(const std::string& jwk_json);
    static std::string build_public_key_pem(const SupabaseJWK& jwk);
    static bool base64_decode(std::string_view input, std::string& output);
    SupabaseJWTVerifierJwtCpp(KeyMaterial current, KeyMaterial standby);
};

}  // namespace infra::security::token

#endif  // INFRA_SECURITY_TOKEN_SUPABASE_JWT_VERIFIER_JWTCPP_H
