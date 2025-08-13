#include "core/security/authentication/Authentication.h"

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>

class AuthenticationTest : public ::testing::Test {
   protected:
    void SetUp() override { auth = std::make_unique<Authentication>(); }

    void TearDown() override { auth.reset(); }

    std::unique_ptr<Authentication> auth;
};

// Test user registration
TEST_F(AuthenticationTest, UserRegistration) {
    // Valid registration
    EXPECT_TRUE(auth->register_user("alice", "alice@example.com", "password123"));

    // Duplicate username should fail
    EXPECT_FALSE(auth->register_user("alice", "alice2@example.com", "password456"));

    // Invalid password (too short)
    EXPECT_FALSE(auth->register_user("bob", "bob@example.com", "12345"));

    // Valid second user
    EXPECT_TRUE(auth->register_user("charlie", "charlie@example.com", "securepass"));
}

// Test user authentication
TEST_F(AuthenticationTest, UserAuthentication) {
    // Register a user first
    ASSERT_TRUE(auth->register_user("alice", "alice@example.com", "password123"));

    // Valid authentication
    EXPECT_TRUE(auth->authenticate_user("alice", "password123"));

    // Invalid password
    EXPECT_FALSE(auth->authenticate_user("alice", "wrongpassword"));

    // Non-existent user
    EXPECT_FALSE(auth->authenticate_user("nonuser", "password123"));

    // Empty credentials
    EXPECT_FALSE(auth->authenticate_user("", "password123"));
    EXPECT_FALSE(auth->authenticate_user("alice", ""));
}

// Test password hashing and verification
TEST_F(AuthenticationTest, PasswordSecurity) {
    std::string password = "mySecretPassword";
    std::string salt = auth->generate_salt();

    // Generate hash
    std::string hash = auth->hash_password(password, salt);
    EXPECT_FALSE(hash.empty());
    EXPECT_NE(hash, password);  // Hash should not equal plain password

    // Verify correct password
    EXPECT_TRUE(auth->verify_password(password, hash, salt));

    // Verify wrong password
    EXPECT_FALSE(auth->verify_password("wrongPassword", hash, salt));

    // Test salt generation
    std::string salt1 = auth->generate_salt();
    std::string salt2 = auth->generate_salt();
    EXPECT_NE(salt1, salt2);  // Salts should be unique
    EXPECT_GE(salt1.length(), 8);  // Salt should have reasonable length
}

// Test user retrieval
TEST_F(AuthenticationTest, UserRetrieval) {
    // Register users
    ASSERT_TRUE(auth->register_user("alice", "alice@example.com", "password123"));
    ASSERT_TRUE(auth->register_user("bob", "bob@example.com", "password456"));

    // Get user by username
    auto alice = auth->get_user_by_username("alice");
    ASSERT_NE(alice, nullptr);
    EXPECT_EQ(alice->username, "alice");
    EXPECT_EQ(alice->email, "alice@example.com");
    EXPECT_TRUE(alice->is_active);

    // Get user by ID
    auto alice_by_id = auth->get_user(alice->id);
    ASSERT_NE(alice_by_id, nullptr);
    EXPECT_EQ(alice_by_id->username, "alice");

    // Non-existent user
    auto nonuser = auth->get_user_by_username("nonuser");
    EXPECT_EQ(nonuser, nullptr);
}

// Test last login update
TEST_F(AuthenticationTest, LastLoginUpdate) {
    // Register and authenticate user
    ASSERT_TRUE(auth->register_user("alice", "alice@example.com", "password123"));
    auto alice = auth->get_user_by_username("alice");
    ASSERT_NE(alice, nullptr);

    auto initial_login = alice->last_login;

    // Wait a bit to ensure time difference
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Update last login
    EXPECT_TRUE(auth->update_last_login(alice->id));

    // Check that last login was updated
    auto updated_alice = auth->get_user_by_username("alice");
    ASSERT_NE(updated_alice, nullptr);
    EXPECT_GT(updated_alice->last_login, initial_login);

    // Invalid user ID
    EXPECT_FALSE(auth->update_last_login("invalid_id"));
}

// Test edge cases and security
TEST_F(AuthenticationTest, SecurityEdgeCases) {
    // Test with special characters in username
    EXPECT_TRUE(auth->register_user("user_123", "user@example.com", "password123"));
    EXPECT_TRUE(auth->authenticate_user("user_123", "password123"));

    // Test with unicode characters
    EXPECT_TRUE(auth->register_user("José", "jose@example.com", "contraseña123"));
    EXPECT_TRUE(auth->authenticate_user("José", "contraseña123"));

    // Test password with special characters
    EXPECT_TRUE(auth->register_user("secure_user", "secure@example.com", "P@ssw0rd!@#$"));
    EXPECT_TRUE(auth->authenticate_user("secure_user", "P@ssw0rd!@#$"));

    // Test very long password
    std::string long_password(1000, 'a');
    EXPECT_TRUE(auth->register_user("long_pass_user", "long@example.com", long_password));
    EXPECT_TRUE(auth->authenticate_user("long_pass_user", long_password));
}

// Test concurrent access
TEST_F(AuthenticationTest, ConcurrentAccess) {
    // Register a new user for testing concurrent access
    ASSERT_TRUE(auth->register_user("concurrent_user", "concurrent@example.com", "concurrent_pass"));
    
    // Test concurrent authentication
    std::vector<std::thread> auth_threads;
    std::atomic<int> success_count(0);
    std::atomic<int> failed_count(0);

    for (int i = 0; i < 5; ++i) {
        auth_threads.emplace_back([this, &success_count, &failed_count]() {
            if (auth->authenticate_user("concurrent_user", "concurrent_pass")) {
                success_count++;
            } else {
                failed_count++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : auth_threads) {
        t.join();
    }

    // Verify that all authentications were processed
    EXPECT_EQ(success_count.load() + failed_count.load(), 5);
    EXPECT_GT(success_count.load(), 0);  // At least some should succeed
}
