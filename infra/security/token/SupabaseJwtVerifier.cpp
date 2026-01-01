#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "infra/security/token/SupabaseJwtVerifier.h"

#include "utils/EnvLoader.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>
#include <sstream>

using json = nlohmann::json;
using namespace utils;

namespace infra::security::token {

std::expected<SupabaseJWTVerifier, JwtVerifyError> SupabaseJWTVerifier::create() {
    auto current = EnvLoader::get_env("SUPABASE_JWT_CURRENT_KEY", "");
    auto standby = EnvLoader::get_env("SUPABASE_JWT_STANDBY_KEY", "");

    if (current.empty() || standby.empty()) {
        return std::unexpected(JwtVerifyError::KeyNotFound);
    }

    auto current_key = parse_jwk(current);
    auto standby_key = parse_jwk(standby);

    if (!current_key || !standby_key) {
        return std::unexpected(JwtVerifyError::JwkParseError);
    }

    return SupabaseJWTVerifier{*current_key, *standby_key};
}

std::optional<SupabaseJWK> SupabaseJWTVerifier::parse_jwk(const std::string& jwk_json) {
    SupabaseJWK jwk;
    try {
        json j = json::parse(jwk_json);
        jwk.kty = j.value("kty", "");
        jwk.alg = j.value("alg", "");
        jwk.kid = j.value("kid", "");
        jwk.x = j.value("x", "");
        jwk.y = j.value("y", "");
        jwk.crv = j.value("crv", "");
        jwk.n = j.value("n", "");
        jwk.e = j.value("e", "");
        return jwk;

    } catch (const std::exception& e) {
        // skip
    }
    return std::nullopt;
}

SupabaseJWTVerifier::SupabaseJWTVerifier(SupabaseJWK current, SupabaseJWK standby)
    : current_key_(std::move(current)), standby_key_(std::move(standby)) {}

JwtVerifyResult SupabaseJWTVerifier::verify_token(const std::string& token) const {
    auto parsed = parse_token(token);
    if (!parsed) return std::unexpected(parsed.error());

    if (verify_with_key(*parsed, current_key_)) return parsed->claims;
    if (verify_with_key(*parsed, standby_key_)) return parsed->claims;

    return std::unexpected(JwtVerifyError::InvalidSignature);
}

std::expected<ParsedJwt, JwtVerifyError> SupabaseJWTVerifier::parse_token(
    const std::string& token) const {
    auto parts = split_token(token);
    if (parts.size() != 3) return std::unexpected(JwtVerifyError::InvalidFormat);

    ParsedJwt parsed;
    parsed.signing_input = parts[0] + "." + parts[1];
    parsed.signature_b64 = parts[2];

    // ---- HEADER ----
    json header = json::parse(base64_decode(parts[0]));
    parsed.alg = header.value("alg", "");
    parsed.kid = header.value("kid", "");

    if (parsed.alg.empty()) return std::unexpected(JwtVerifyError::UnsupportedAlgorithm);

    // ---- PAYLOAD (THIS IS THE ANSWER TO YOUR QUESTION) ----
    json payload = json::parse(base64_decode(parts[1]));

    UserClaims claims;
    claims.id = payload.value("sub", "");
    claims.email = payload.value("email", "");
    claims.aud = payload.value("aud", "");
    claims.iss = payload.value("iss", "");
    claims.exp = payload.value("exp", 0);
    claims.iat = payload.value("iat", 0);

    // REQUIRED claims
    if (claims.id.empty() || claims.email.empty())
        return std::unexpected(JwtVerifyError::MissingClaims);

    // ---------- OPTIONAL USER METADATA ----------
    if (payload.contains("user_metadata") && payload["user_metadata"].is_object()) {
        const auto& meta = payload["user_metadata"];
        claims.username = meta.value("username", "");
        claims.full_name = meta.value("full_name", "");
    }

    // ---------- OPTIONAL APP METADATA ----------
    if (payload.contains("app_metadata") && payload["app_metadata"].is_object()) {
        const auto& app_meta = payload["app_metadata"];
        if (app_meta.contains("provider")) {
            claims.role = "authenticated";
        }
    }

    // expiration check belongs HERE
    auto now = std::chrono::system_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    if (claims.exp < sec) return std::unexpected(JwtVerifyError::TokenExpired);

    parsed.claims = std::move(claims);
    return parsed;
}

std::vector<std::string> SupabaseJWTVerifier::split_token(const std::string& token) const {
    std::vector<std::string> parts;
    std::stringstream ss(token);
    std::string part;

    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    return parts;
}

bool SupabaseJWTVerifier::verify_with_key(const ParsedJwt& jwt, const SupabaseJWK& key) const {
    if (jwt.alg != key.alg) {
        log(LogLevel::WARN, "Algorithm mismatch: token alg ", jwt.alg, ", key alg ", key.alg);
        return false;
    }

    if (jwt.alg == "ES256") {
        return verify_es256_signature(jwt.signing_input, jwt.signature_b64, key);
    } else {
        log(LogLevel::WARN, "Unsupported algorithm: " + jwt.alg);
        return false;
    }
}

std::string SupabaseJWTVerifier::base64_decode(const std::string& input) const {
    std::string decoded = input;

    // Replace URL-safe characters
    for (char& c : decoded) {
        if (c == '-') c = '+';
        if (c == '_') c = '/';
    }

    // Add padding if needed
    while (decoded.length() % 4 != 0) {
        decoded += '=';
    }

    // Simple base64 decode implementation
    const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string result;
    int val = 0, valb = -8;

    for (char c : decoded) {
        if (c == '=') break;

        size_t pos = base64_chars.find(c);
        if (pos == std::string::npos) continue;

        val = (val << 6) + pos;
        valb += 6;

        if (valb >= 0) {
            result.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return result;
}

bool SupabaseJWTVerifier::verify_es256_signature(const std::string& header_payload,
                                                 const std::string& signature_b64,
                                                 const SupabaseJWK& key) const {
    try {
        // Decode signature
        std::string signature = base64_decode(signature_b64);

        // ES256 signatures in JWT are raw R and S components concatenated (not DER encoded)
        // Each component is 32 bytes for P-256 curve
        if (signature.length() != 64) {
            log(LogLevel::ERROR, "ES256 verification - signature length should be 64 bytes, got " +
                                     std::to_string(signature.length()));
            return false;
        }

        // Split into R and S components (each 32 bytes)
        std::string r_bytes = signature.substr(0, 32);
        std::string s_bytes = signature.substr(32, 32);

        // Create EC key from JWK
        EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
        if (!ec_key) {
            log(LogLevel::ERROR, "ES256 verification - failed to create EC key");
            return false;
        }

        // Set public key from x, y coordinates
        BIGNUM* x = BN_new();
        BIGNUM* y = BN_new();

        std::string x_bytes = base64_decode(key.x);
        std::string y_bytes = base64_decode(key.y);

        BN_bin2bn((unsigned char*)x_bytes.c_str(), x_bytes.length(), x);
        BN_bin2bn((unsigned char*)y_bytes.c_str(), y_bytes.length(), y);

        int set_result = EC_KEY_set_public_key_affine_coordinates(ec_key, x, y);
        if (set_result != 1) {
            log(LogLevel::ERROR, "ES256 verification - failed to set public key coordinates");
            BN_free(x);
            BN_free(y);
            EC_KEY_free(ec_key);
            return false;
        }

        // Create ECDSA signature
        ECDSA_SIG* sig = ECDSA_SIG_new();
        BIGNUM* r = BN_new();
        BIGNUM* s = BN_new();

        BN_bin2bn((unsigned char*)r_bytes.c_str(), r_bytes.length(), r);
        BN_bin2bn((unsigned char*)s_bytes.c_str(), s_bytes.length(), s);

        ECDSA_SIG_set0(sig, r, s);

        // Hash the header_payload using SHA256 (required for ES256)
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, (unsigned char*)header_payload.c_str(), header_payload.length());
        SHA256_Final(hash, &sha256);

        // Verify signature
        int result = ECDSA_do_verify(hash, SHA256_DIGEST_LENGTH, sig, ec_key);

        // Cleanup
        ECDSA_SIG_free(sig);
        BN_free(x);
        BN_free(y);
        EC_KEY_free(ec_key);

        return result == 1;

    } catch (const std::exception& e) {
        log(LogLevel::ERROR, "ES256 verification error: " + std::string(e.what()));
        return false;
    }
}

}  // namespace infra::security::token

#pragma GCC diagnostic pop
