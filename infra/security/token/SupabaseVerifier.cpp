#include "infra/security/token/SupabaseVerifier.h"

#include "utils/EnvLoader.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
using json = nlohmann::json;
namespace infra::security::token {
namespace {
bool fill_claims(const jwt::decoded_jwt<jwt::traits::kazuho_picojson>& decoded,
                 UserClaims& claims) {
    claims = {};
    if (!decoded.has_subject() || !decoded.has_issuer() || !decoded.has_audience() ||
        !decoded.has_expires_at())
        return false;
    try {
        claims.id = decoded.get_subject();
        claims.iss = decoded.get_issuer();
        const auto aud_claim = decoded.get_payload_claim("aud");
        if (aud_claim.get_type() == jwt::json::type::string) {
            claims.aud = aud_claim.as_string();
        } else if (aud_claim.get_type() == jwt::json::type::array) {
            const auto arr = aud_claim.as_array();
            if (arr.empty() || !arr.front().is<std::string>()) return false;
            claims.aud = arr.front().get<std::string>();
        } else {
            return false;
        }
        const auto exp_claim = decoded.get_payload_claim("exp");
        if (exp_claim.get_type() != jwt::json::type::integer) return false;
        claims.exp = exp_claim.as_integer();
        if (decoded.has_payload_claim("iat")) {
            const auto iat_claim = decoded.get_payload_claim("iat");
            if (iat_claim.get_type() == jwt::json::type::integer) {
                claims.iat = iat_claim.as_integer();
            }
        }
        if (decoded.has_payload_claim("role")) {
            const auto role_claim = decoded.get_payload_claim("role");
            if (role_claim.get_type() == jwt::json::type::string) {
                claims.role = role_claim.as_string();
            }
        }
        if (decoded.has_payload_claim("email")) {
            const auto email_claim = decoded.get_payload_claim("email");
            if (email_claim.get_type() == jwt::json::type::string) {
                claims.email = email_claim.as_string();
            }
        }
        if (decoded.has_payload_claim("user_metadata")) {
            const auto meta_claim = decoded.get_payload_claim("user_metadata");
            const auto meta = meta_claim.to_json();
            if (meta.is<jwt::traits::kazuho_picojson::object_type>()) {
                const auto& obj = meta.get<jwt::traits::kazuho_picojson::object_type>();
                auto it = obj.find("username");
                if (it != obj.end() && it->second.is<std::string>()) {
                    claims.username = it->second.get<std::string>();
                }
                it = obj.find("full_name");
                if (it != obj.end() && it->second.is<std::string>()) {
                    claims.full_name = it->second.get<std::string>();
                }
            }
        }
    } catch (const std::exception&) {
        return false;
    }
    return !claims.id.empty() && claims.exp != 0 && !claims.iss.empty() && !claims.aud.empty();
}
const std::string& expected_issuer() {
    static const std::string value = utils::EnvLoader::get_env("SUPABASE_EXPECTED_ISS", "");
    return value;
}
const std::string& expected_audience() {
    static const std::string value = utils::EnvLoader::get_env("SUPABASE_EXPECTED_AUD", "");
    return value;
}
bool decode_b64url(std::string_view input, std::string& output) {
    try {
        output = jwt::base::decode<jwt::alphabet::base64url>(
            jwt::base::pad<jwt::alphabet::base64url>(std::string(input)));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
bool verify_es256_signature(evp_pkey_st* pkey, std::string_view header_b64,
                            std::string_view payload_b64, std::string_view signature_raw) {
    if (!pkey || signature_raw.size() != 64) return false;
    ECDSA_SIG* sig = ECDSA_SIG_new();
    if (!sig) return false;
    const unsigned char* sig_bytes = reinterpret_cast<const unsigned char*>(signature_raw.data());
    BIGNUM* r = BN_bin2bn(sig_bytes, 32, nullptr);
    BIGNUM* s = BN_bin2bn(sig_bytes + 32, 32, nullptr);
    if (!r || !s) {
        BN_free(r);
        BN_free(s);
        ECDSA_SIG_free(sig);
        return false;
    }
    if (ECDSA_SIG_set0(sig, r, s) != 1) {
        BN_free(r);
        BN_free(s);
        ECDSA_SIG_free(sig);
        return false;
    }
    int der_len = i2d_ECDSA_SIG(sig, nullptr);
    if (der_len <= 0) {
        ECDSA_SIG_free(sig);
        return false;
    }
    std::array<unsigned char, 72> der{};
    unsigned char* p = der.data();
    if (i2d_ECDSA_SIG(sig, &p) != der_len) {
        ECDSA_SIG_free(sig);
        return false;
    }
    ECDSA_SIG_free(sig);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;
    const EVP_MD* md = EVP_sha256();
    if (!md) {
        EVP_MD_CTX_free(ctx);
        return false;
    }
    if (EVP_DigestVerifyInit(ctx, nullptr, md, nullptr, pkey) <= 0) {
        EVP_MD_CTX_free(ctx);
        return false;
    }
    if (EVP_DigestVerifyUpdate(ctx, header_b64.data(), header_b64.size()) <= 0) {
        EVP_MD_CTX_free(ctx);
        return false;
    }
    const char dot = '.';
    if (EVP_DigestVerifyUpdate(ctx, &dot, 1) <= 0) {
        EVP_MD_CTX_free(ctx);
        return false;
    }
    if (EVP_DigestVerifyUpdate(ctx, payload_b64.data(), payload_b64.size()) <= 0) {
        EVP_MD_CTX_free(ctx);
        return false;
    }
    int result = EVP_DigestVerifyFinal(ctx, der.data(), der_len);
    EVP_MD_CTX_free(ctx);
    return result == 1;
}
}  // namespace
std::expected<SupabaseVerifier, JwtVerifyError> SupabaseVerifier::create() {
    auto current = utils::EnvLoader::get_env("SUPABASE_JWT_CURRENT_KEY", "");
    if (current.empty()) {
        std::cout << "Supabase current JWK not found in environment variables." << std::endl;
        return std::unexpected(JwtVerifyError::KeyNotFound);
    }
    auto current_jwk = parse_jwk_json(current);
    if (!current_jwk || current_jwk->kid.empty()) {
        std::cout << "Failed to parse Supabase current JWK." << std::endl;
        return std::unexpected(JwtVerifyError::JwkParseError);
    }
    std::unordered_map<std::string, KeyMaterial, StringHash, StringEq> keys;
    std::vector<std::string> key_order;
    auto current_key = build_public_key(*current_jwk);
    if (!current_key) {
        std::cout << "Failed to build public key from current JWK." << std::endl;
        return std::unexpected(JwtVerifyError::JwkParseError);
    }
    keys.emplace(current_jwk->kid, KeyMaterial{*current_jwk, std::move(current_key)});
    key_order.push_back(current_jwk->kid);
    auto standby = utils::EnvLoader::get_env("SUPABASE_JWT_STANDBY_KEY", "");
    if (!standby.empty()) {
        auto standby_jwk = parse_jwk_json(standby);
        if (!standby_jwk || standby_jwk->kid.empty()) {
            std::cout << "Failed to parse Supabase standby JWK." << std::endl;
            return std::unexpected(JwtVerifyError::JwkParseError);
        }
        auto standby_key = build_public_key(*standby_jwk);
        if (!standby_key) {
            std::cout << "Failed to build public key from standby JWK." << std::endl;
            return std::unexpected(JwtVerifyError::JwkParseError);
        }
        keys.emplace(standby_jwk->kid, KeyMaterial{*standby_jwk, std::move(standby_key)});
        key_order.push_back(standby_jwk->kid);
    }
    return SupabaseVerifier{std::move(keys), std::move(key_order)};
}
SupabaseVerifier::SupabaseVerifier(
    std::unordered_map<std::string, KeyMaterial, StringHash, StringEq> keys,
    std::vector<std::string> key_order)
    : keys_(std::move(keys)), key_order_(std::move(key_order)) {}
std::optional<SupabaseJWK> SupabaseVerifier::parse_jwk_json(const std::string& jwk_json) {
    SupabaseJWK jwk;
    json j = json::parse(jwk_json, nullptr, false);
    if (j.is_discarded()) return std::nullopt;
    jwk.kty = j.value("kty", "");
    jwk.alg = j.value("alg", "");
    jwk.kid = j.value("kid", "");
    jwk.x = j.value("x", "");
    jwk.y = j.value("y", "");
    jwk.crv = j.value("crv", "");
    jwk.n = j.value("n", "");
    jwk.e = j.value("e", "");
    return jwk;
}
std::shared_ptr<evp_pkey_st> SupabaseVerifier::build_public_key(const SupabaseJWK& jwk) {
    if (jwk.x.empty() || jwk.y.empty()) {
        std::cout << "JWK missing x/y coordinates." << std::endl;
        return {};
    }
    std::string x_bytes;
    std::string y_bytes;
    if (!decode_b64url(jwk.x, x_bytes) || !decode_b64url(jwk.y, y_bytes)) {
        std::cout << "Failed to decode base64url x/y." << std::endl;
        return {};
    }
    if (x_bytes.size() != 32 || y_bytes.size() != 32) {
        std::cout << "Unexpected x/y size for P-256 key." << std::endl;
        return {};
    }
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ec_key) {
        std::cout << "EC_KEY_new_by_curve_name failed." << std::endl;
        return {};
    }
    BIGNUM* x = BN_bin2bn(reinterpret_cast<const unsigned char*>(x_bytes.data()),
                          static_cast<int>(x_bytes.size()), nullptr);
    BIGNUM* y = BN_bin2bn(reinterpret_cast<const unsigned char*>(y_bytes.data()),
                          static_cast<int>(y_bytes.size()), nullptr);
    if (!x || !y) {
        std::cout << "BN_bin2bn failed for x/y." << std::endl;
        BN_free(x);
        BN_free(y);
        EC_KEY_free(ec_key);
        return {};
    }
    int set_result = EC_KEY_set_public_key_affine_coordinates(ec_key, x, y);
    BN_free(x);
    BN_free(y);
    if (set_result != 1) {
        std::cout << "EC_KEY_set_public_key_affine_coordinates failed." << std::endl;
        EC_KEY_free(ec_key);
        return {};
    }
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        std::cout << "EVP_PKEY_new failed." << std::endl;
        EC_KEY_free(ec_key);
        return {};
    }
    if (EVP_PKEY_assign_EC_KEY(pkey, ec_key) != 1) {
        std::cout << "EVP_PKEY_assign_EC_KEY failed." << std::endl;
        EVP_PKEY_free(pkey);
        EC_KEY_free(ec_key);
        return {};
    }
    return std::shared_ptr<evp_pkey_st>(pkey, EVP_PKEY_free);
}
JwtVerifyResult SupabaseVerifier::verify_token(std::string_view token) const {
    if (token.empty()) return std::unexpected(JwtVerifyError::EmptyToken);
    try {
        auto decoded = jwt::decode(std::string(token));
        const std::string alg = decoded.get_algorithm();
        if (alg != "ES256") return std::unexpected(JwtVerifyError::UnsupportedAlgorithm);
        std::string kid;
        if (decoded.has_header_claim("kid")) kid = decoded.get_key_id();
        const auto& header_b64 = decoded.get_header_base64();
        const auto& payload_b64 = decoded.get_payload_base64();
        const auto& signature_raw = decoded.get_signature();
        auto verify_with_key = [&](const KeyMaterial& key) -> bool {
            if (!key.pkey) return false;
            if (!key.jwk.alg.empty() && key.jwk.alg != alg) {
                log(utils::LogLevel::WARN, "Algorithm mismatch: token alg ", alg, ", key alg ",
                    key.jwk.alg);
                return false;
            }
            return verify_es256_signature(key.pkey.get(), header_b64, payload_b64, signature_raw);
        };
        if (!kid.empty()) {
            auto it = keys_.find(kid);
            if (it == keys_.end()) return std::unexpected(JwtVerifyError::KeyNotFound);
            if (!verify_with_key(it->second)) {
                return std::unexpected(JwtVerifyError::InvalidSignature);
            }
        } else {
            bool verified = false;
            for (const auto& key_id : key_order_) {
                auto it = keys_.find(key_id);
                if (it != keys_.end() && verify_with_key(it->second)) {
                    verified = true;
                    break;
                }
            }
            if (!verified) return std::unexpected(JwtVerifyError::InvalidSignature);
        }
        UserClaims claims;
        if (!fill_claims(decoded, claims)) return std::unexpected(JwtVerifyError::MissingClaims);
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        if (decoded.has_payload_claim("nbf")) {
            const auto nbf_claim = decoded.get_payload_claim("nbf");
            if (nbf_claim.get_type() != jwt::json::type::integer) {
                return std::unexpected(JwtVerifyError::MissingClaims);
            }
            if (sec < nbf_claim.as_integer()) {
                return std::unexpected(JwtVerifyError::TokenNotYetValid);
            }
        }
        if (claims.exp < sec) return std::unexpected(JwtVerifyError::TokenExpired);
        const auto& expected_iss = expected_issuer();
        if (!expected_iss.empty() && claims.iss != expected_iss) {
            return std::unexpected(JwtVerifyError::IssuerMismatch);
        }
        const auto& expected_aud = expected_audience();
        if (!expected_aud.empty() && claims.aud != expected_aud) {
            return std::unexpected(JwtVerifyError::AudienceMismatch);
        }
        return claims;
    } catch (const std::exception&) {
        return std::unexpected(JwtVerifyError::InvalidFormat);
    }
}
}  // namespace infra::security::token
