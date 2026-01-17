#include "infra/security/token/SupabaseJwtVerifier.h"

#include "infra/security/token/JwtJsonUtils.h"
#include "utils/EnvLoader.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>

#define DER_SIGNATURE_MAX_LEN 72  // max length of DER-encoded ECDSA signature

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

void log_openssl_errors(const char* where) {
    unsigned long err = 0;
    while ((err = ERR_get_error()) != 0) {
        char buf[256];
        ERR_error_string_n(err, buf, sizeof(buf));
        std::cout << where << " OpenSSL error: " << buf << std::endl;
    }
}

}  // namespace

std::expected<SupabaseJWTVerifier, JwtVerifyError> SupabaseJWTVerifier::create() {
    auto current = EnvLoader::get_env("SUPABASE_JWT_CURRENT_KEY", "");
    auto standby = EnvLoader::get_env("SUPABASE_JWT_STANDBY_KEY", "");
    if (current.empty() || standby.empty()) {
        std::cout << "Supabase JWKs not found in environment variables." << std::endl;
        return std::unexpected(JwtVerifyError::KeyNotFound);
    }

    auto current_key = parse_jwk(current);
    auto standby_key = parse_jwk(standby);

    if (!current_key || !standby_key) {
        std::cout << "Failed to parse Supabase JWKs." << std::endl;
        return std::unexpected(JwtVerifyError::JwkParseError);
    }

    return SupabaseJWTVerifier{*current_key, *standby_key};
}

std::optional<SupabaseJWTVerifier::KeyMaterial> SupabaseJWTVerifier::parse_jwk(
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

    auto pkey = build_public_key(jwk);
    if (!pkey) {
        std::cout << "Failed to build public key from JWK." << std::endl;
        return std::nullopt;
    }
    return KeyMaterial{std::move(jwk), std::move(pkey)};
}

std::shared_ptr<evp_pkey_st> SupabaseJWTVerifier::build_public_key(const SupabaseJWK& jwk) {
    if (jwk.x.empty() || jwk.y.empty()) {
        std::cout << "JWK missing x/y coordinates." << std::endl;
        return {};
    }
    std::string x_bytes;
    std::string y_bytes;
    if (!base64_decode(jwk.x, x_bytes) || !base64_decode(jwk.y, y_bytes)) {
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

SupabaseJWTVerifier::SupabaseJWTVerifier(KeyMaterial current, KeyMaterial standby)
    : current_key_(std::move(current)), standby_key_(std::move(standby)) {}

JwtVerifyResult SupabaseJWTVerifier::verify_token(std::string_view token) const {
    auto parsed = parse_token(token);
    if (!parsed) return std::unexpected(parsed.error());

    const ParsedJwt& jwt = *parsed;

    // ---- Fast path: kid present ----
    if (!jwt.kid.empty()) {
        if (jwt.kid == current_key_.jwk.kid) {
            if (verify_with_key(jwt, current_key_)) return jwt.claims;
            return std::unexpected(JwtVerifyError::InvalidSignature);
        }

        if (jwt.kid == standby_key_.jwk.kid) {
            if (verify_with_key(jwt, standby_key_)) return jwt.claims;
            return std::unexpected(JwtVerifyError::InvalidSignature);
        }

        // kid does not match any known key → hard fail
        return std::unexpected(JwtVerifyError::InvalidSignature);
    }

    // ---- Slow path: no kid, try both ----
    if (verify_with_key(jwt, current_key_)) return jwt.claims;

    if (verify_with_key(jwt, standby_key_)) return jwt.claims;

    return std::unexpected(JwtVerifyError::InvalidSignature);
}

std::expected<ParsedJwt, JwtVerifyError> SupabaseJWTVerifier::parse_token(
    std::string_view token) const {
    std::string_view h64, p64, s64, signing;
    if (!split_token(token, h64, p64, s64, signing))
        return std::unexpected(JwtVerifyError::InvalidFormat);

    ParsedJwt out;
    out.signing_input = signing;
    out.signature_b64 = s64;

    std::string decoded;
    decoded.reserve(512);

    // ---- header ----
    if (!base64_decode(h64, decoded)) return std::unexpected(JwtVerifyError::InvalidFormat);

    std::string_view alg, kid;
    if (!jwt_scan::parse_header(decoded, alg, kid))
        return std::unexpected(JwtVerifyError::UnsupportedAlgorithm);

    out.alg.assign(alg);
    out.kid.assign(kid);

    // ---- payload ----
    decoded.clear();
    if (!base64_decode(p64, decoded)) return std::unexpected(JwtVerifyError::InvalidFormat);

    UserClaims claims;
    if (!jwt_scan::parse_payload(decoded, claims))
        return std::unexpected(JwtVerifyError::MissingClaims);

    auto now = std::chrono::system_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    if (claims.exp < sec) return std::unexpected(JwtVerifyError::TokenExpired);

    out.claims = std::move(claims);
    return out;
}

bool SupabaseJWTVerifier::split_token(std::string_view token, std::string_view& header,
                                      std::string_view& payload, std::string_view& signature,
                                      std::string_view& signing_input) const {
    size_t first = token.find('.');
    if (first == std::string_view::npos) {
        return false;
    }
    size_t second = token.find('.', first + 1);
    if (second == std::string_view::npos) {
        return false;
    }
    if (token.find('.', second + 1) != std::string_view::npos) {
        return false;
    }

    header = token.substr(0, first);
    payload = token.substr(first + 1, second - first - 1);
    signature = token.substr(second + 1);
    signing_input = token.substr(0, second);
    return true;
}

bool SupabaseJWTVerifier::verify_with_key(const ParsedJwt& jwt, const KeyMaterial& key) const {
    if (jwt.alg != key.jwk.alg) {
        log(LogLevel::WARN, "Algorithm mismatch: token alg ", jwt.alg, ", key alg ", key.jwk.alg);
        return false;
    }

    if (!key.pkey) {
        log(LogLevel::WARN, "Missing public key material for verification");
        return false;
    }

    if (jwt.alg == "ES256") {
        return verify_es256_signature(jwt.signing_input, jwt.signature_b64, key);
    } else {
        log(LogLevel::WARN, "Unsupported algorithm: " + jwt.alg);
        return false;
    }
}

bool SupabaseJWTVerifier::base64_decode(std::string_view input, std::string& output) {
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

bool SupabaseJWTVerifier::verify_es256_signature(std::string_view header_payload,
                                                 std::string_view signature_b64,
                                                 const KeyMaterial& key) const {
    std::string signature;
    if (!base64_decode(signature_b64, signature)) {
        log(LogLevel::ERROR, "ES256 verification - failed to decode signature");
        return false;
    }

    // ES256 signatures in JWT are raw R and S components concatenated (not DER encoded).
    if (signature.length() != 64) {
        log(LogLevel::ERROR, "ES256 verification - signature length should be 64 bytes, got " +
                                 std::to_string(signature.length()));
        return false;
    }

    ECDSA_SIG* sig = ECDSA_SIG_new();
    if (!sig) {
        log(LogLevel::ERROR, "ES256 verification - failed to allocate ECDSA_SIG");
        return false;
    }

    const unsigned char* sig_bytes = reinterpret_cast<const unsigned char*>(signature.data());
    BIGNUM* r = BN_bin2bn(sig_bytes, 32, nullptr);
    BIGNUM* s = BN_bin2bn(sig_bytes + 32, 32, nullptr);
    if (!r || !s) {
        BN_free(r);
        BN_free(s);
        ECDSA_SIG_free(sig);
        log(LogLevel::ERROR, "ES256 verification - failed to parse signature");
        return false;
    }

    if (ECDSA_SIG_set0(sig, r, s) != 1) {
        BN_free(r);
        BN_free(s);
        ECDSA_SIG_free(sig);
        log(LogLevel::ERROR, "ES256 verification - failed to set signature components");
        return false;
    }

    int der_len = i2d_ECDSA_SIG(sig, nullptr);
    if (der_len <= 0) {
        ECDSA_SIG_free(sig);
        log(LogLevel::ERROR, "ES256 verification - failed to encode signature");
        return false;
    }

    unsigned char der[DER_SIGNATURE_MAX_LEN];  // ES256 max DER size
    unsigned char* p = der;
    if (i2d_ECDSA_SIG(sig, &p) != der_len) {
        ECDSA_SIG_free(sig);
        log(LogLevel::ERROR, "ES256 verification - signature encoding mismatch");
        return false;
    }
    ECDSA_SIG_free(sig);

    thread_local EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        log(LogLevel::ERROR, "ES256 verification - failed to allocate EVP_MD_CTX");
        return false;
    }

    const EVP_MD* md = EVP_sha256();
    if (!md) {
        EVP_MD_CTX_reset(ctx);
        log(LogLevel::ERROR, "ES256 verification - EVP_sha256 returned null");
        log_openssl_errors("EVP_sha256");
        return false;
    }

    if (EVP_DigestVerifyInit(ctx, nullptr, md, nullptr, key.pkey.get()) <= 0) {
        log(LogLevel::ERROR, "ES256 verification - key type: ",
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            EVP_PKEY_get0_type_name(key.pkey.get())
#else
            EVP_PKEY_base_id(key.pkey.get())
#endif
        );
        EVP_MD_CTX_reset(ctx);
        log(LogLevel::ERROR, "ES256 verification - EVP_DigestVerifyInit failed");
        log_openssl_errors("EVP_DigestVerifyInit");
        return false;
    }

    if (EVP_DigestVerifyUpdate(ctx, header_payload.data(), header_payload.size()) <= 0) {
        EVP_MD_CTX_reset(ctx);
        log(LogLevel::ERROR, "ES256 verification - EVP_DigestVerifyUpdate failed");
        log_openssl_errors("EVP_DigestVerifyUpdate");
        return false;
    }

    int result = EVP_DigestVerifyFinal(ctx, reinterpret_cast<const unsigned char*>(der), der_len);
    EVP_MD_CTX_reset(ctx);

    return result == 1;
}

}  // namespace infra::security::token
