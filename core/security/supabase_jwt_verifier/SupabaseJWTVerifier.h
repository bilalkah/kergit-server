#pragma once
#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

struct SupabaseUser {
    std::string id;
    std::string email;
    std::string username;
    std::string full_name;
    std::string role;
    std::string aud;  // audience
    std::string iss;  // issuer
    int64_t exp{0};   // expiration time (epoch seconds)
    int64_t iat{0};   // issued at time (epoch seconds)

    // helper to convert UNIX epoch -> readable UTC time
    static std::string to_utc_string(int64_t epoch) {
        if (epoch == 0) return "";
        std::time_t t = static_cast<std::time_t>(epoch);
        std::tm tm = *std::gmtime(&t);  // convert to UTC
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S UTC");
        return oss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const SupabaseUser& u) {
        os << "SupabaseUser {\n";
        if (!u.id.empty()) os << "  id: " << u.id << "\n";
        if (!u.email.empty()) os << "  email: " << u.email << "\n";
        if (!u.username.empty()) os << "  username: " << u.username << "\n";
        if (!u.full_name.empty()) os << "  full_name: " << u.full_name << "\n";
        if (!u.role.empty()) os << "  role: " << u.role << "\n";
        if (!u.aud.empty()) os << "  aud: " << u.aud << "\n";
        if (!u.iss.empty()) os << "  iss: " << u.iss << "\n";
        if (u.exp != 0) os << "  exp: " << u.exp << " (" << to_utc_string(u.exp) << ")\n";
        if (u.iat != 0) os << "  iat: " << u.iat << " (" << to_utc_string(u.iat) << ")\n";
        os << "}";
        return os;
    }
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
    bool verify_es256_signature(const std::string& header_payload, const std::string& signature_b64,
                                const JWK& key);
    std::vector<std::string> split_token(const std::string& token);
    int64_t get_current_timestamp();
    JWK parse_jwk(const std::string& jwk_json);
};