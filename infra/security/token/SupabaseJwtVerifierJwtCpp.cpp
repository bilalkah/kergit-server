#include "infra/security/token/SupabaseJwtVerifierJwtCpp.h"

#include "utils/EnvLoader.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>

using json = nlohmann::json;
using namespace utils;

namespace infra::security::token {
namespace {
const std::array<int8_t, 256>& base64url_table() {
    static const std::array<int8_t, 256> table = [] {
        std::array<int8_t, 256> t{};
        t.fill(-1);
        for (int i = 0; i < 26; ++i) {
            t[static_cast<unsigned char>('A' + i)] = static_cast<int8_t>(i);
            t[static_cast<unsigned char>('a' + i)] = static_cast<int8_t>(26 + i);
        }
        for (int i = 0; i < 10; ++i) {
            t[static_cast<unsigned char>('0' + i)] = static_cast<int8_t>(52 + i);
        }
        t[static_cast<unsigned char>('+')] = 62;
        t[static_cast<unsigned char>('/')] = 63;
        t[static_cast<unsigned char>('-')] = 62;
        t[static_cast<unsigned char>('_')] = 63;
        return t;
    }();
    return table;
}
bool fill_claims(const jwt::decoded_jwt<jwt::traits::kazuho_picojson>& decoded,
                 UserClaims& claims) {
    claims = {};

    if (!decoded.has_subject() || !decoded.has_issuer() || !decoded.has_audience() ||
        !decoded.has_expires_at()) {
        return false;
    }

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
}  // namespace

std::expected<SupabaseJWTVerifierJwtCpp, JwtVerifyError> SupabaseJWTVerifierJwtCpp::create() {
    auto current = EnvLoader::get_env("SUPABASE_JWT_CURRENT_KEY", "");
    auto standby = EnvLoader::get_env("SUPABASE_JWT_STANDBY_KEY", "");
    if (current.empty() || standby.empty()) {
        std::cout << "Supabase JWKs not found in environment variables (jwt-cpp)." << std::endl;
        return std::unexpected(JwtVerifyError::KeyNotFound);
    }

    auto current_key = parse_jwk(current);
    auto standby_key = parse_jwk(standby);

    if (!current_key || !standby_key) {
        std::cout << "Failed to parse Supabase JWKs (jwt-cpp)." << std::endl;
        return std::unexpected(JwtVerifyError::JwkParseError);
    }

    return SupabaseJWTVerifierJwtCpp{*current_key, *standby_key};
}

SupabaseJWTVerifierJwtCpp::SupabaseJWTVerifierJwtCpp(KeyMaterial current, KeyMaterial standby)
    : current_key_(std::move(current)), standby_key_(std::move(standby)) {}

std::optional<SupabaseJWTVerifierJwtCpp::KeyMaterial> SupabaseJWTVerifierJwtCpp::parse_jwk(
    const std::string& jwk_json) {
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

    auto pem = build_public_key_pem(jwk);
    if (pem.empty()) {
        std::cout << "Failed to build public key from JWK (jwt-cpp)." << std::endl;
        return std::nullopt;
    }
    try {
        jwt::algorithm::es256 verifier{pem};
        return KeyMaterial{std::move(jwk), std::move(verifier)};
    } catch (const std::exception&) {
        std::cout << "Failed to initialize ES256 verifier (jwt-cpp)." << std::endl;
        return std::nullopt;
    }
}

std::string SupabaseJWTVerifierJwtCpp::build_public_key_pem(const SupabaseJWK& jwk) {
    if (jwk.x.empty() || jwk.y.empty()) {
        std::cout << "JWK missing x/y coordinates (jwt-cpp)." << std::endl;
        return {};
    }
    std::string x_bytes;
    std::string y_bytes;
    if (!base64_decode(jwk.x, x_bytes) || !base64_decode(jwk.y, y_bytes)) {
        std::cout << "Failed to decode base64url x/y (jwt-cpp)." << std::endl;
        return {};
    }
    if (x_bytes.size() != 32 || y_bytes.size() != 32) {
        std::cout << "Unexpected x/y size for P-256 key (jwt-cpp)." << std::endl;
        return {};
    }

    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ec_key) {
        std::cout << "EC_KEY_new_by_curve_name failed (jwt-cpp)." << std::endl;
        return {};
    }

    BIGNUM* x = BN_bin2bn(reinterpret_cast<const unsigned char*>(x_bytes.data()),
                          static_cast<int>(x_bytes.size()), nullptr);
    BIGNUM* y = BN_bin2bn(reinterpret_cast<const unsigned char*>(y_bytes.data()),
                          static_cast<int>(y_bytes.size()), nullptr);
    if (!x || !y) {
        std::cout << "BN_bin2bn failed for x/y (jwt-cpp)." << std::endl;
        BN_free(x);
        BN_free(y);
        EC_KEY_free(ec_key);
        return {};
    }

    int set_result = EC_KEY_set_public_key_affine_coordinates(ec_key, x, y);
    BN_free(x);
    BN_free(y);
    if (set_result != 1) {
        std::cout << "EC_KEY_set_public_key_affine_coordinates failed (jwt-cpp)." << std::endl;
        EC_KEY_free(ec_key);
        return {};
    }

    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        std::cout << "EVP_PKEY_new failed (jwt-cpp)." << std::endl;
        EC_KEY_free(ec_key);
        return {};
    }
    if (EVP_PKEY_assign_EC_KEY(pkey, ec_key) != 1) {
        std::cout << "EVP_PKEY_assign_EC_KEY failed (jwt-cpp)." << std::endl;
        EVP_PKEY_free(pkey);
        EC_KEY_free(ec_key);
        return {};
    }

    BIO* mem = BIO_new(BIO_s_mem());
    if (!mem) {
        std::cout << "BIO_new failed (jwt-cpp)." << std::endl;
        EVP_PKEY_free(pkey);
        return {};
    }
    if (PEM_write_bio_PUBKEY(mem, pkey) != 1) {
        std::cout << "PEM_write_bio_PUBKEY failed (jwt-cpp)." << std::endl;
        BIO_free(mem);
        EVP_PKEY_free(pkey);
        return {};
    }

    char* data = nullptr;
    long len = BIO_get_mem_data(mem, &data);
    std::string pem;
    if (len > 0 && data) {
        pem.assign(data, static_cast<size_t>(len));
    }
    BIO_free(mem);
    EVP_PKEY_free(pkey);

    return pem;
}

bool SupabaseJWTVerifierJwtCpp::base64_decode(std::string_view input, std::string& output) {
    output.clear();
    output.reserve((input.size() * 3) / 4 + 1);

    int val = 0;
    int valb = -8;
    const auto& table = base64url_table();

    for (unsigned char c : input) {
        if (c == '=') break;
        int8_t d = table[c];
        if (d < 0) return false;

        val = (val << 6) + d;
        valb += 6;

        if (valb >= 0) {
            output.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return true;
}

JwtVerifyResult SupabaseJWTVerifierJwtCpp::verify_token(std::string_view token) const {
    try {
        auto decoded = jwt::decode(std::string(token));

        const std::string alg = decoded.get_algorithm();
        std::string kid;
        if (decoded.has_header_claim("kid")) {
            kid = decoded.get_key_id();
        }

        UserClaims claims;
        if (!fill_claims(decoded, claims)) return std::unexpected(JwtVerifyError::MissingClaims);

        auto now = std::chrono::system_clock::now();
        auto sec =
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        if (claims.exp < sec) return std::unexpected(JwtVerifyError::TokenExpired);

        const std::string signing_input =
            decoded.get_header_base64() + "." + decoded.get_payload_base64();
        const std::string& signature = decoded.get_signature();

        auto verify_with_key = [&](const KeyMaterial& key) -> bool {
            if (alg != key.jwk.alg) {
                log(LogLevel::WARN, "Algorithm mismatch: token alg ", alg, ", key alg ",
                    key.jwk.alg);
                return false;
            }

            std::error_code ec;
            key.verifier.verify(signing_input, signature, ec);
            return !ec;
        };

        if (!kid.empty()) {
            if (kid == current_key_.jwk.kid) {
                if (verify_with_key(current_key_)) return claims;
                return std::unexpected(JwtVerifyError::InvalidSignature);
            }

            if (kid == standby_key_.jwk.kid) {
                if (verify_with_key(standby_key_)) return claims;
                return std::unexpected(JwtVerifyError::InvalidSignature);
            }

            return std::unexpected(JwtVerifyError::InvalidSignature);
        }

        if (verify_with_key(current_key_)) return claims;
        if (verify_with_key(standby_key_)) return claims;

        return std::unexpected(JwtVerifyError::InvalidSignature);
    } catch (const std::exception&) {
        return std::unexpected(JwtVerifyError::InvalidFormat);
    }
}

}  // namespace infra::security::token
