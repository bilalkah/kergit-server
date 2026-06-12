#ifndef INFRA_SECURITY_TOKEN_SUPABASE_VERIFIER_H
#define INFRA_SECURITY_TOKEN_SUPABASE_VERIFIER_H

#include "infra/security/token/ITokenVerifier.h"
#include "infra/security/token/JwtTypes.h"
#include "utils/Loggable.h"

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct evp_pkey_st;

namespace infra::security::token {

class SupabaseVerifier : public utils::Loggable, public ITokenVerifier {
   public:
    static std::expected<SupabaseVerifier, JwtVerifyError> create(std::string expected_issuer = "");

    ~SupabaseVerifier() = default;

    JwtVerifyResult verify_token(std::string_view token) const override;

   private:
    struct KeyMaterial {
        SupabaseJWK jwk;
        std::shared_ptr<evp_pkey_st> pkey;
    };

    struct StringHash {
        using is_transparent = void;
        size_t operator()(std::string_view v) const noexcept {
            return std::hash<std::string_view>{}(v);
        }
        size_t operator()(const std::string& v) const noexcept {
            return std::hash<std::string_view>{}(v);
        }
    };

    struct StringEq {
        using is_transparent = void;
        bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
    };

    std::unordered_map<std::string, KeyMaterial, StringHash, StringEq> keys_;
    std::vector<std::string> key_order_;
    std::string expected_issuer_;
    std::string expected_audience_;

    static std::optional<SupabaseJWK> parse_jwk_json(const std::string& jwk_json);
    static std::shared_ptr<evp_pkey_st> build_public_key(const SupabaseJWK& jwk);
    SupabaseVerifier(std::unordered_map<std::string, KeyMaterial, StringHash, StringEq> keys,
                     std::vector<std::string> key_order, std::string expected_issuer,
                     std::string expected_audience);
};

}  // namespace infra::security::token

#endif  // INFRA_SECURITY_TOKEN_SUPABASE_VERIFIER_H
