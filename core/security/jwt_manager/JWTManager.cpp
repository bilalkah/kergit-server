#include "core/security/jwt_manager/JWTManager.h"

#include <chrono>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

JWTManager::JWTManager(const std::string& secret_key) : secret_key_(secret_key) {}

JWTManager::~JWTManager() {}

std::string JWTManager::generate_token(const JWTClaims& claims) {
    // Create header
    json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };
    
    // Create payload
    json payload = {
        {"user_id", claims.user_id},
        {"username", claims.username},
        {"role", claims.role},
        {"session_id", claims.session_id},
        {"iat", std::chrono::duration_cast<std::chrono::seconds>(
            claims.issued_at.time_since_epoch()).count()},
        {"exp", std::chrono::duration_cast<std::chrono::seconds>(
            claims.expires_at.time_since_epoch()).count()}
    };
    
    // Encode header and payload
    std::string encoded_header = base64_encode(header.dump());
    std::string encoded_payload = base64_encode(payload.dump());
    
    // Create signature
    std::string message = encoded_header + "." + encoded_payload;
    std::string signature = hmac_sha256(message, secret_key_);
    std::string encoded_signature = base64_encode(signature);
    
    return encoded_header + "." + encoded_payload + "." + encoded_signature;
}

bool JWTManager::verify_token(const std::string& token) {
    try {
        // Split token into parts
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = token.find('.');
        
        while (end != std::string::npos) {
            parts.push_back(token.substr(start, end - start));
            start = end + 1;
            end = token.find('.', start);
        }
        parts.push_back(token.substr(start));
        
        if (parts.size() != 3) {
            return false;
        }
        
        // Verify signature
        std::string message = parts[0] + "." + parts[1];
        std::string expected_signature = hmac_sha256(message, secret_key_);
        std::string expected_signature_b64 = base64_encode(expected_signature);
        
        // Compare base64 encoded signatures
        if (expected_signature_b64.length() != parts[2].length()) {
            return false;
        }
        
        int result = 0;
        for (size_t i = 0; i < expected_signature_b64.length(); ++i) {
            result |= expected_signature_b64[i] ^ parts[2][i];
        }
        
        return result == 0;
    } catch (const std::exception&) {
        return false;
    }
}

JWTClaims JWTManager::decode_token(const std::string& token) {
    JWTClaims claims{};
    
    try {
        // Split token into parts
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = token.find('.');
        
        while (end != std::string::npos) {
            parts.push_back(token.substr(start, end - start));
            start = end + 1;
            end = token.find('.', start);
        }
        parts.push_back(token.substr(start));
        
        if (parts.size() != 3) {
            return claims;  // Return empty claims on error
        }
        
        // Decode payload
        std::string payload_json = base64_decode(parts[1]);
        json payload = json::parse(payload_json);
        
        // Extract claims
        if (payload.contains("user_id")) {
            claims.user_id = payload["user_id"];
        }
        if (payload.contains("username")) {
            claims.username = payload["username"];
        }
        if (payload.contains("role")) {
            claims.role = payload["role"];
        }
        if (payload.contains("session_id")) {
            claims.session_id = payload["session_id"];
        }
        if (payload.contains("iat")) {
            long long iat = payload["iat"].get<long long>();
            claims.issued_at = std::chrono::system_clock::from_time_t(static_cast<time_t>(iat));
        }
        if (payload.contains("exp")) {
            long long exp = payload["exp"].get<long long>();
            claims.expires_at = std::chrono::system_clock::from_time_t(static_cast<time_t>(exp));
        }
        
    } catch (const std::exception&) {
        // Return empty claims on any error
        claims = JWTClaims{};
    }
    
    return claims;
}

bool JWTManager::is_token_expired(const std::string& token) {
    JWTClaims claims = decode_token(token);
    if (claims.user_id.empty()) {
        return true;  // Invalid token is considered expired
    }
    
    auto now = std::chrono::system_clock::now();
    return now >= claims.expires_at;
}

bool JWTManager::is_token_valid(const std::string& token) {
    // Check if token is revoked
    auto it = revoked_tokens_.find(token);
    bool is_revoked = (it != revoked_tokens_.end() && it->second);
    
    return verify_token(token) && !is_token_expired(token) && !is_revoked;
}

void JWTManager::rotate_secret() {
    // Generate a new random secret key
    // In a real implementation, this would use a proper random generator
    std::string new_secret = "rotated_secret_" + std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    
    secret_key_ = new_secret;
    
    // Clear revoked tokens as they were signed with old secret
    revoked_tokens_.clear();
}

bool JWTManager::revoke_token(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    
    // Add token to revoked list
    revoked_tokens_[token] = true;
    return true;
}

std::string JWTManager::base64_encode(const std::string& input) {
    static const std::string chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        encoded.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }
    return encoded;
}

std::string JWTManager::base64_decode(const std::string& input) {
    static const int decode_table[128] = {
        -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
        52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-2,-1,-1,
        -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
        15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
        -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
        41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
    };
    
    std::string decoded;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (decode_table[c] == -1) break;
        val = (val << 6) + decode_table[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

std::string JWTManager::hmac_sha256(const std::string& data, const std::string& key) {
    unsigned char result[SHA256_DIGEST_LENGTH];
    unsigned int result_len;
    
    HMAC(EVP_sha256(), 
         key.c_str(), static_cast<int>(key.length()),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         result, &result_len);
    
    return std::string(reinterpret_cast<char*>(result), result_len);
}
