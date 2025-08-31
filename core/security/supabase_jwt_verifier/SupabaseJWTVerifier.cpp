#include "SupabaseJWTVerifier.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

SupabaseJWTVerifier::SupabaseJWTVerifier(const std::string& current_key_jwk, const std::string& standby_key_jwk)
    : current_key_(parse_jwk(current_key_jwk)), standby_key_(parse_jwk(standby_key_jwk)) {
}

SupabaseJWTVerifier::~SupabaseJWTVerifier() {
}

std::optional<SupabaseUser> SupabaseJWTVerifier::verify_token(const std::string& token) {
    if (token.empty()) {
        std::cerr << "[SUPABASE_JWT] Empty token provided" << std::endl;
        return std::nullopt;
    }

    std::cerr << "[SUPABASE_JWT] Token length: " << token.length() << std::endl;
    std::cerr << "[SUPABASE_JWT] Token preview: " << token.substr(0, 50) << "..." << std::endl;
    std::cerr << "[SUPABASE_JWT] Attempting verification with key kid: " << current_key_.kid << std::endl;

    // Try current key first
    auto user = decode_and_verify(token, current_key_);
    if (user.has_value()) {
        std::cerr << "[SUPABASE_JWT] Token verified successfully" << std::endl;
        return user;
    }

    std::cerr << "[SUPABASE_JWT] Token verification failed" << std::endl;
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

std::optional<SupabaseUser> SupabaseJWTVerifier::decode_and_verify(const std::string& token, const JWK& key) {
    try {
        auto parts = split_token(token);
        if (parts.size() != 3) {
            std::cerr << "[SUPABASE_JWT] Invalid token format - expected 3 parts" << std::endl;
            return std::nullopt;
        }

        std::string header_b64 = parts[0];
        std::string payload_b64 = parts[1];
        std::string signature_b64 = parts[2];

        std::cerr << "[SUPABASE_JWT] Token parts - header: " << header_b64.substr(0, 20) << "..."
                  << ", payload: " << payload_b64.substr(0, 20) << "..."
                  << ", signature: " << signature_b64.substr(0, 20) << "..." << std::endl;

        // Decode header to check algorithm
        std::string header_json = base64_decode(header_b64);
        json header = json::parse(header_json);
        std::string alg = header.value("alg", "");
        std::string kid = header.value("kid", "");

        std::cerr << "[SUPABASE_JWT] Token algorithm: " << alg << ", key id: " << kid << std::endl;

        // Verify signature based on algorithm
        std::string header_payload = header_b64 + "." + payload_b64;
        bool signature_valid = false;

        if (alg == "ES256") {
            signature_valid = verify_es256_signature(header_payload, signature_b64, key);
        } else {
            std::cerr << "[SUPABASE_JWT] Unsupported algorithm: " << alg << std::endl;
            return std::nullopt;
        }

        if (!signature_valid) {
            std::cerr << "[SUPABASE_JWT] Signature verification failed" << std::endl;
            return std::nullopt;
        }

        // Decode payload
        std::string payload_json = base64_decode(payload_b64);
        json payload = json::parse(payload_json);

        // Extract user information
        SupabaseUser user;
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
            std::cerr << "[SUPABASE_JWT] Missing required user fields" << std::endl;
            return std::nullopt;
        }

        return user;

    } catch (const std::exception& e) {
        std::cerr << "[SUPABASE_JWT] Error decoding token: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::string SupabaseJWTVerifier::base64_decode(const std::string& input) {
    // Simple base64url decode (Supabase uses base64url)
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

bool SupabaseJWTVerifier::verify_es256_signature(const std::string& header_payload, const std::string& signature_b64, const JWK& key) {
    try {
        std::cerr << "[SUPABASE_JWT] ES256 verification - header_payload length: " << header_payload.length() << std::endl;
        std::cerr << "[SUPABASE_JWT] ES256 verification - signature_b64 length: " << signature_b64.length() << std::endl;
        
        // Decode signature
        std::string signature = base64_decode(signature_b64);
        std::cerr << "[SUPABASE_JWT] ES256 verification - decoded signature length: " << signature.length() << std::endl;
        
        // ES256 signatures in JWT are raw R and S components concatenated (not DER encoded)
        // Each component is 32 bytes for P-256 curve
        if (signature.length() != 64) {
            std::cerr << "[SUPABASE_JWT] ES256 verification - signature length should be 64 bytes, got " << signature.length() << std::endl;
            return false;
        }
        
        // Split into R and S components (each 32 bytes)
        std::string r_bytes = signature.substr(0, 32);
        std::string s_bytes = signature.substr(32, 32);
        
        std::cerr << "[SUPABASE_JWT] ES256 verification - R component length: " << r_bytes.length() << std::endl;
        std::cerr << "[SUPABASE_JWT] ES256 verification - S component length: " << s_bytes.length() << std::endl;
        
        // Create EC key from JWK
        EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
        if (!ec_key) {
            std::cerr << "[SUPABASE_JWT] ES256 verification - failed to create EC key" << std::endl;
            return false;
        }
        
        // Set public key from x, y coordinates
        BIGNUM* x = BN_new();
        BIGNUM* y = BN_new();
        
        std::string x_bytes = base64_decode(key.x);
        std::string y_bytes = base64_decode(key.y);
        
        std::cerr << "[SUPABASE_JWT] ES256 verification - x_bytes length: " << x_bytes.length() << std::endl;
        std::cerr << "[SUPABASE_JWT] ES256 verification - y_bytes length: " << y_bytes.length() << std::endl;
        
        BN_bin2bn((unsigned char*)x_bytes.c_str(), x_bytes.length(), x);
        BN_bin2bn((unsigned char*)y_bytes.c_str(), y_bytes.length(), y);
        
        int set_result = EC_KEY_set_public_key_affine_coordinates(ec_key, x, y);
        if (set_result != 1) {
            std::cerr << "[SUPABASE_JWT] ES256 verification - failed to set public key coordinates" << std::endl;
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
        
        std::cerr << "[SUPABASE_JWT] ES256 verification - header_payload hash length: " << SHA256_DIGEST_LENGTH << std::endl;
        
        // Verify signature
        int result = ECDSA_do_verify(hash, SHA256_DIGEST_LENGTH, sig, ec_key);
        
        std::cerr << "[SUPABASE_JWT] ES256 verification - ECDSA_do_verify result: " << result << std::endl;
        
        // Cleanup
        ECDSA_SIG_free(sig);
        BN_free(x);
        BN_free(y);
        EC_KEY_free(ec_key);
        
        return result == 1;
        
    } catch (const std::exception& e) {
        std::cerr << "[SUPABASE_JWT] ES256 verification error: " << e.what() << std::endl;
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

JWK SupabaseJWTVerifier::parse_jwk(const std::string& jwk_json) {
    JWK jwk;
    try {
        std::cerr << "[SUPABASE_JWT] Parsing JWK JSON: " << jwk_json.substr(0, 100) << "..." << std::endl;
        json j = json::parse(jwk_json);
        jwk.kty = j.value("kty", "");
        jwk.alg = j.value("alg", "");
        jwk.kid = j.value("kid", "");
        jwk.x = j.value("x", "");
        jwk.y = j.value("y", "");
        jwk.crv = j.value("crv", "");
        jwk.n = j.value("n", "");
        jwk.e = j.value("e", "");
        
        std::cerr << "[SUPABASE_JWT] Parsed JWK - kty: " << jwk.kty << ", alg: " << jwk.alg << ", kid: " << jwk.kid << std::endl;
        std::cerr << "[SUPABASE_JWT] Parsed JWK - x length: " << jwk.x.length() << ", y length: " << jwk.y.length() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[SUPABASE_JWT] Error parsing JWK: " << e.what() << std::endl;
        std::cerr << "[SUPABASE_JWT] JWK JSON that failed: " << jwk_json << std::endl;
    }
    return jwk;
} 