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

SupabaseJWTVerifier::SupabaseJWTVerifier() {
    std::string current_jwk = EnvLoader::get_env("SUPABASE_JWT_CURRENT_KEY", "");
    std::string standby_jwk = EnvLoader::get_env("SUPABASE_JWT_STANDBY_KEY", "");
    if (current_jwk.empty() || standby_jwk.empty()) {
        log(LogLevel::ERROR, "JWK keys not set in environment variables");
        throw std::runtime_error("JWK keys not set in environment variables");
    }
    current_key_ = parse_jwk(current_jwk);
    standby_key_ = parse_jwk(standby_jwk);
}

SupabaseJWTVerifier::~SupabaseJWTVerifier() {}

std::optional<UserClaims> SupabaseJWTVerifier::verify_token(const std::string& token) {
    if (token.empty()) {
        log(LogLevel::WARN, "Empty token provided for verification");
        return std::nullopt;
    }

    // Try current key first
    auto user = decode_and_verify(token, current_key_);
    if (user.has_value()) {
        log(LogLevel::INFO, "Token verified with current key");
        return user;
    }

    // Fallback to standby key
    user = decode_and_verify(token, standby_key_);
    if (user.has_value()) {
        log(LogLevel::INFO, "Token verified with standby key");
        return user;
    }
    log(LogLevel::WARN, "Token verification failed with both keys");

    return std::nullopt;
}

bool SupabaseJWTVerifier::is_token_valid(const std::string& token) {
    auto user = verify_token(token);
    if (!user.has_value()) {
        return false;
    }
    return !is_token_expired(token);
}

bool SupabaseJWTVerifier::is_token_expired(const std::string& token) {
    auto user = verify_token(token);
    if (!user.has_value()) {
        return true;
    }

    int64_t current_time = get_current_timestamp();
    return user->exp < current_time;
}

std::optional<UserClaims> SupabaseJWTVerifier::decode_and_verify(const std::string& token,
                                                                 const SupabaseJWK& key) {
    try {
        auto parts = split_token(token);
        if (parts.size() != 3) {
            log(LogLevel::WARN, "Invalid token format - expected 3 parts");
            return std::nullopt;
        }

        std::string header_b64 = parts[0];
        std::string payload_b64 = parts[1];
        std::string signature_b64 = parts[2];

        // Decode header to check algorithm
        std::string header_json = base64_decode(header_b64);
        json header = json::parse(header_json);
        std::string alg = header.value("alg", "");
        std::string kid = header.value("kid", "");

        // Verify signature based on algorithm
        std::string header_payload = header_b64 + "." + payload_b64;
        bool signature_valid = false;

        if (alg == "ES256") {
            signature_valid = verify_es256_signature(header_payload, signature_b64, key);
        } else {
            log(LogLevel::WARN, "Unsupported algorithm: " + alg);
            return std::nullopt;
        }

        if (!signature_valid) {
            log(LogLevel::WARN, "Signature verification failed");
            return std::nullopt;
        }

        // Decode payload
        std::string payload_json = base64_decode(payload_b64);
        json payload = json::parse(payload_json);

        // Extract user information
        UserClaims user;
        user.id = payload.value("sub", "");
        user.email = payload.value("email", "");
        user.aud = payload.value("aud", "");
        user.iss = payload.value("iss", "");
        user.exp = payload.value("exp", 0);
        user.iat = payload.value("iat", 0);

        // Extract user metadata
        if (payload.contains("user_metadata")) {
            json metadata = payload["user_metadata"];
            user.username = metadata.value("username", "");
            user.full_name = metadata.value("full_name", "");
        }

        // Extract app metadata for role
        if (payload.contains("app_metadata")) {
            json app_metadata = payload["app_metadata"];
            if (app_metadata.contains("provider")) {
                user.role = "authenticated";
            }
        }

        // Validate required fields
        if (user.id.empty() || user.email.empty()) {
            log(LogLevel::WARN, "Missing required user fields");
            return std::nullopt;
        }

        log(LogLevel::INFO, "Token successfully decoded and verified. Decoded user: ", user);

        return user;

    } catch (const std::exception& e) {
        log(LogLevel::ERROR, "Error decoding and verifying token: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::string SupabaseJWTVerifier::base64_decode(const std::string& input) {
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
                                                 const SupabaseJWK& key) {
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

std::vector<std::string> SupabaseJWTVerifier::split_token(const std::string& token) {
    std::vector<std::string> parts;
    std::stringstream ss(token);
    std::string part;

    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    return parts;
}

int64_t SupabaseJWTVerifier::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

SupabaseJWK SupabaseJWTVerifier::parse_jwk(const std::string& jwk_json) {
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

    } catch (const std::exception& e) {
        log(LogLevel::ERROR,
            "Error parsing JWK: " + std::string(e.what()) + "\nJWK JSON that failed: " + jwk_json);
    }
    return jwk;
}

}  // namespace infra::security::token