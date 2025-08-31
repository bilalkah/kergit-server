#pragma once
#include <string>
#include <vector>
#include <optional>

struct SupabaseUser {
    std::string id;
    std::string email;
    std::string username;
    std::string full_name;
    std::string role;
    std::string aud;  // audience
    std::string iss;  // issuer
    int64_t exp;      // expiration time
    int64_t iat;      // issued at time
};

struct JWK {
    std::string kty;  // key type
    std::string alg;  // algorithm
    std::string kid;  // key id
    std::string x;    // x coordinate (for EC keys)
    std::string y;    // y coordinate (for EC keys)
    std::string crv;  // curve (for EC keys)
    std::string n;    // modulus (for RSA keys)
    std::string e;    // exponent (for RSA keys)
};

class SupabaseJWTVerifier {
public:
    SupabaseJWTVerifier(const std::string& current_key_jwk, const std::string& standby_key_jwk);
    ~SupabaseJWTVerifier();

    // Verify JWT token and extract user information
    std::optional<SupabaseUser> verify_token(const std::string& token);
    
    // Check if token is valid (not expired, properly signed)
    bool is_token_valid(const std::string& token);
    
    // Check if token is expired
    bool is_token_expired(const std::string& token);

private:
    JWK current_key_;
    JWK standby_key_;
    
    // Helper methods
    std::optional<SupabaseUser> decode_and_verify(const std::string& token, const JWK& key);
    std::string base64_decode(const std::string& input);
    bool verify_es256_signature(const std::string& header_payload, const std::string& signature_b64, const JWK& key);
    std::vector<std::string> split_token(const std::string& token);
    int64_t get_current_timestamp();
    JWK parse_jwk(const std::string& jwk_json);
}; 