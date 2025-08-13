#include "core/security/hmac_validator/HMACValidator.h"

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>

class HMACValidatorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        secret_key = "test_secret_key_12345";
        validator = std::make_unique<HMACValidator>(secret_key);
    }

    void TearDown() override { validator.reset(); }

    std::string secret_key;
    std::unique_ptr<HMACValidator> validator;
};

// Test HMAC generation
TEST_F(HMACValidatorTest, HMACGeneration) {
    std::string message = "Hello, World!";
    
    // Generate HMAC
    std::string hmac = validator->generate_hmac(message);
    EXPECT_FALSE(hmac.empty());
    EXPECT_GT(hmac.length(), 0);

    // Same message should produce same HMAC
    std::string hmac2 = validator->generate_hmac(message);
    EXPECT_EQ(hmac, hmac2);

    // Different message should produce different HMAC
    std::string different_hmac = validator->generate_hmac("Different message");
    EXPECT_NE(hmac, different_hmac);
}

// Test HMAC verification
TEST_F(HMACValidatorTest, HMACVerification) {
    std::string message = "Test message for verification";
    
    // Generate HMAC and verify
    std::string hmac = validator->generate_hmac(message);
    EXPECT_TRUE(validator->verify_hmac(message, hmac));

    // Verify with wrong message
    EXPECT_FALSE(validator->verify_hmac("Wrong message", hmac));

    // Verify with wrong HMAC
    std::string wrong_hmac = "wrong_hmac_value";
    EXPECT_FALSE(validator->verify_hmac(message, wrong_hmac));

    // Verify with empty values
    EXPECT_FALSE(validator->verify_hmac("", hmac));
    EXPECT_FALSE(validator->verify_hmac(message, ""));
}

// Test timestamped HMAC
TEST_F(HMACValidatorTest, TimestampedHMAC) {
    std::string message = "Timestamped message";
    
    // Generate timestamped HMAC
    std::string timestamped_hmac = validator->generate_hmac_with_timestamp(message);
    EXPECT_FALSE(timestamped_hmac.empty());
    EXPECT_GT(timestamped_hmac.length(), 0);

    // Verify immediately (should succeed)
    EXPECT_TRUE(validator->verify_hmac_with_timestamp(message, timestamped_hmac, 60));

    // Different message should fail
    EXPECT_FALSE(validator->verify_hmac_with_timestamp("Different message", timestamped_hmac, 60));
}

// Test timestamp expiration
TEST_F(HMACValidatorTest, TimestampExpiration) {
    std::string message = "Expiring message";
    
    // Generate timestamped HMAC
    std::string timestamped_hmac = validator->generate_hmac_with_timestamp(message);
    
    // Should be valid within time window
    EXPECT_TRUE(validator->verify_hmac_with_timestamp(message, timestamped_hmac, 10));
    
    // Should be invalid with very short time window
    EXPECT_FALSE(validator->verify_hmac_with_timestamp(message, timestamped_hmac, 0));
}

// Test replay attack prevention
TEST_F(HMACValidatorTest, ReplayAttackPrevention) {
    std::string message = "Important message";
    std::string message_id = "msg_001";
    std::string timestamp = "1234567890";
    
    // Test replay attack detection
    EXPECT_FALSE(validator->is_replay_attack(message_id, timestamp));
    
    // Generate and sign message
    std::string signature = validator->sign_message(message);
    EXPECT_FALSE(signature.empty());
    EXPECT_TRUE(validator->verify_message_signature(message, signature));
    
    // Wrong signature should fail
    EXPECT_FALSE(validator->verify_message_signature(message, "wrong_signature"));
}

// Test different key sizes
TEST_F(HMACValidatorTest, DifferentKeySizes) {
    std::string message = "Test message";
    
    // Short key
    HMACValidator short_key_validator("short");
    std::string short_hmac = short_key_validator.generate_hmac(message);
    EXPECT_FALSE(short_hmac.empty());
    EXPECT_TRUE(short_key_validator.verify_hmac(message, short_hmac));
    
    // Long key
    std::string long_key(256, 'k');
    HMACValidator long_key_validator(long_key);
    std::string long_hmac = long_key_validator.generate_hmac(message);
    EXPECT_FALSE(long_hmac.empty());
    EXPECT_TRUE(long_key_validator.verify_hmac(message, long_hmac));
    
    // Different keys should produce different HMACs
    EXPECT_NE(short_hmac, long_hmac);
    EXPECT_NE(short_hmac, validator->generate_hmac(message));
}

// Test binary data
TEST_F(HMACValidatorTest, BinaryData) {
    // Test with binary data (null bytes, high-bit characters)
    std::vector<uint8_t> binary_data = {0x00, 0x01, 0xFF, 0x7F, 0x80, 0xDE, 0xAD, 0xBE, 0xEF};
    std::string binary_message(binary_data.begin(), binary_data.end());
    
    std::string hmac = validator->generate_hmac(binary_message);
    EXPECT_FALSE(hmac.empty());
    EXPECT_TRUE(validator->verify_hmac(binary_message, hmac));
    
    // Modify one byte and verify it fails
    binary_data[0] = 0x02;
    std::string modified_message(binary_data.begin(), binary_data.end());
    EXPECT_FALSE(validator->verify_hmac(modified_message, hmac));
}

// Test large data
TEST_F(HMACValidatorTest, LargeData) {
    // Test with large message
    std::string large_message(10000, 'A');
    
    std::string hmac = validator->generate_hmac(large_message);
    EXPECT_FALSE(hmac.empty());
    EXPECT_TRUE(validator->verify_hmac(large_message, hmac));
    
    // Modify one character in the middle
    large_message[5000] = 'B';
    EXPECT_FALSE(validator->verify_hmac(large_message, hmac));
}

// Test edge cases
TEST_F(HMACValidatorTest, EdgeCases) {
    // Empty message
    std::string empty_hmac = validator->generate_hmac("");
    EXPECT_FALSE(empty_hmac.empty());
    EXPECT_TRUE(validator->verify_hmac("", empty_hmac));
    
    // Very long message
    std::string very_long_message(1000000, 'X');
    std::string very_long_hmac = validator->generate_hmac(very_long_message);
    EXPECT_FALSE(very_long_hmac.empty());
    EXPECT_TRUE(validator->verify_hmac(very_long_message, very_long_hmac));
    
    // Unicode message
    std::string unicode_message = "Hello 世界 🌍 Привет мир";
    std::string unicode_hmac = validator->generate_hmac(unicode_message);
    EXPECT_FALSE(unicode_hmac.empty());
    EXPECT_TRUE(validator->verify_hmac(unicode_message, unicode_hmac));
}

// Test thread safety
TEST_F(HMACValidatorTest, ThreadSafety) {
    std::vector<std::thread> threads;
    std::vector<bool> results(10, false);
    
    // Generate and verify HMACs concurrently
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &results]() {
            std::string message = "Message " + std::to_string(i);
            std::string hmac = validator->generate_hmac(message);
            results[i] = validator->verify_hmac(message, hmac);
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All verifications should succeed
    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}

// Test key rotation
TEST_F(HMACValidatorTest, KeyRotation) {
    std::string message = "Test message";
    
    // Generate HMAC with original key
    std::string original_hmac = validator->generate_hmac(message);
    EXPECT_TRUE(validator->verify_hmac(message, original_hmac));
    
    // Rotate key
    std::string new_key = "new_secret_key_67890";
    validator->rotate_key(new_key);
    
    // Generate HMAC with new key
    std::string new_hmac = validator->generate_hmac(message);
    EXPECT_TRUE(validator->verify_hmac(message, new_hmac));
    
    // Old HMAC should no longer verify (different key)
    EXPECT_FALSE(validator->verify_hmac(message, original_hmac));
    
    // New and old HMACs should be different
    EXPECT_NE(original_hmac, new_hmac);
}
