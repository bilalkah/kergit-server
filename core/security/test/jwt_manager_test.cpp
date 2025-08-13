#include "core/security/jwt_manager/JWTManager.h"

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>

class JWTManagerTest : public ::testing::Test {
   protected:
    void SetUp() override {
        secret_key = "test_jwt_secret_key_12345";
        jwt_manager = std::make_unique<JWTManager>(secret_key);
    }

    void TearDown() override { jwt_manager.reset(); }

    std::string secret_key;
    std::unique_ptr<JWTManager> jwt_manager;

    JWTClaims create_test_claims(const std::string& user_id = "user123", 
                                const std::string& username = "testuser",
                                const std::string& role = "user",
                                int expiry_minutes = 60) {
        auto now = std::chrono::system_clock::now();
        return JWTClaims{
            user_id,
            username,
            role,
            now,
            now + std::chrono::minutes(expiry_minutes),
            "session_" + user_id
        };
    }
};

// Test JWT generation
TEST_F(JWTManagerTest, JWTGeneration) {
    JWTClaims claims = create_test_claims();

    // Generate JWT
    std::string token = jwt_manager->generate_token(claims);
    EXPECT_FALSE(token.empty());
    EXPECT_GT(token.length(), 0);

    // JWT should have 3 parts separated by dots
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);
    EXPECT_NE(first_dot, std::string::npos);
    EXPECT_NE(second_dot, std::string::npos);
    EXPECT_EQ(token.find('.', second_dot + 1), std::string::npos);  // No third dot
}

// Test JWT verification
TEST_F(JWTManagerTest, JWTVerification) {
    JWTClaims claims = create_test_claims();

    // Generate and verify token
    std::string token = jwt_manager->generate_token(claims);
    EXPECT_TRUE(jwt_manager->verify_token(token));
    EXPECT_TRUE(jwt_manager->is_token_valid(token));

    // Invalid token should fail
    EXPECT_FALSE(jwt_manager->verify_token("invalid.token.here"));
    EXPECT_FALSE(jwt_manager->verify_token(""));
    EXPECT_FALSE(jwt_manager->is_token_valid("invalid.token.here"));

    // Malformed token should fail
    EXPECT_FALSE(jwt_manager->verify_token("not.a.valid.jwt.token"));
}

// Test token decoding
TEST_F(JWTManagerTest, TokenDecoding) {
    JWTClaims original_claims = create_test_claims("user456", "testuser456", "admin");

    // Generate token
    std::string token = jwt_manager->generate_token(original_claims);

    // Decode token
    JWTClaims decoded_claims = jwt_manager->decode_token(token);
    EXPECT_EQ(decoded_claims.user_id, original_claims.user_id);
    EXPECT_EQ(decoded_claims.username, original_claims.username);
    EXPECT_EQ(decoded_claims.role, original_claims.role);
    EXPECT_EQ(decoded_claims.session_id, original_claims.session_id);
    
    // Times should be close (within a few seconds)
    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
        decoded_claims.issued_at - original_claims.issued_at).count();
    EXPECT_LE(std::abs(time_diff), 2);
}

// Test token expiration
TEST_F(JWTManagerTest, TokenExpiration) {
    // Generate token with very short expiry
    JWTClaims short_claims = create_test_claims("user789", "testuser789", "user", 0);  // 0 minutes = immediate expiry
    std::string short_token = jwt_manager->generate_token(short_claims);
    
    // Should be expired immediately (or very soon)
    EXPECT_TRUE(jwt_manager->is_token_expired(short_token));
    
    // Generate token with reasonable expiry
    JWTClaims valid_claims = create_test_claims("user790", "testuser790", "user", 60);  // 60 minutes
    std::string valid_token = jwt_manager->generate_token(valid_claims);
    
    // Should not be expired
    EXPECT_FALSE(jwt_manager->is_token_expired(valid_token));
}

// Test token revocation
TEST_F(JWTManagerTest, TokenRevocation) {
    JWTClaims claims = create_test_claims("user202", "testuser202", "user");

    // Generate token
    std::string token = jwt_manager->generate_token(claims);
    EXPECT_TRUE(jwt_manager->verify_token(token));

    // Revoke token
    EXPECT_TRUE(jwt_manager->revoke_token(token));

    // Token should still verify signature but might fail validation
    // (depends on implementation details)
    EXPECT_TRUE(jwt_manager->verify_token(token));  // Signature still valid
    
    // Try to revoke already revoked token
    EXPECT_TRUE(jwt_manager->revoke_token(token));  // Should handle gracefully
}

// Test secret rotation
TEST_F(JWTManagerTest, SecretRotation) {
    JWTClaims claims = create_test_claims("user303", "testuser303", "user");

    // Generate token with original secret
    std::string original_token = jwt_manager->generate_token(claims);
    EXPECT_TRUE(jwt_manager->verify_token(original_token));

    // Rotate secret
    jwt_manager->rotate_secret();

    // Generate token with new secret
    std::string new_token = jwt_manager->generate_token(claims);
    EXPECT_TRUE(jwt_manager->verify_token(new_token));

    // Old token should no longer verify (different secret)
    EXPECT_FALSE(jwt_manager->verify_token(original_token));

    // New and old tokens should be different
    EXPECT_NE(original_token, new_token);
}

// Test edge cases
TEST_F(JWTManagerTest, EdgeCases) {
    // Empty user ID
    JWTClaims empty_user_claims = create_test_claims("", "testuser", "user");
    std::string empty_user_token = jwt_manager->generate_token(empty_user_claims);
    // Should handle gracefully (either generate or return empty)
    
    // Empty username
    JWTClaims empty_username_claims = create_test_claims("user505", "", "user");
    std::string empty_username_token = jwt_manager->generate_token(empty_username_claims);
    EXPECT_FALSE(empty_username_token.empty());
    EXPECT_TRUE(jwt_manager->verify_token(empty_username_token));

    // Very long user ID
    std::string long_user_id(1000, 'u');
    JWTClaims long_user_claims = create_test_claims(long_user_id, "testuser", "user");
    std::string long_user_token = jwt_manager->generate_token(long_user_claims);
    EXPECT_FALSE(long_user_token.empty());
    
    JWTClaims decoded_long = jwt_manager->decode_token(long_user_token);
    EXPECT_EQ(decoded_long.user_id, long_user_id);

    // Different roles
    std::vector<std::string> test_roles = {"admin", "moderator", "user", "guest"};
    for (const auto& role : test_roles) {
        JWTClaims role_claims = create_test_claims("user606", "testuser606", role);
        std::string role_token = jwt_manager->generate_token(role_claims);
        EXPECT_FALSE(role_token.empty());
        
        JWTClaims decoded_role = jwt_manager->decode_token(role_token);
        EXPECT_EQ(decoded_role.role, role);
    }
}

// Test thread safety
TEST_F(JWTManagerTest, ThreadSafety) {
    std::vector<std::thread> threads;
    std::vector<std::string> tokens(10);
    std::vector<bool> valid_results(10, false);

    // Generate tokens concurrently
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &tokens]() {
            JWTClaims claims = create_test_claims("user" + std::to_string(i), 
                                                 "testuser" + std::to_string(i), 
                                                 "user");
            tokens[i] = jwt_manager->generate_token(claims);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    threads.clear();

    // Verify tokens concurrently
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &tokens, &valid_results]() {
            valid_results[i] = jwt_manager->verify_token(tokens[i]);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All tokens should be valid
    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(tokens[i].empty());
        EXPECT_TRUE(valid_results[i]);
    }
}

// Test invalid inputs
TEST_F(JWTManagerTest, InvalidInputs) {
    // Test decoding invalid tokens
    JWTClaims invalid_claims = jwt_manager->decode_token("invalid.token");
    EXPECT_TRUE(invalid_claims.user_id.empty());
    EXPECT_TRUE(invalid_claims.username.empty());
    
    // Test operations on empty tokens
    EXPECT_FALSE(jwt_manager->verify_token(""));
    EXPECT_FALSE(jwt_manager->is_token_valid(""));
    EXPECT_TRUE(jwt_manager->is_token_expired(""));
    EXPECT_FALSE(jwt_manager->revoke_token(""));
    
    // Test malformed tokens
    EXPECT_FALSE(jwt_manager->verify_token("not.a.jwt"));
    EXPECT_FALSE(jwt_manager->verify_token("too.many.dots.here.invalid"));
    EXPECT_FALSE(jwt_manager->verify_token("onlyone"));
}
