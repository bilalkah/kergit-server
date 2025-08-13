#include "core/security/rate_limiter/RateLimiter.h"

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>

class RateLimiterTest : public ::testing::Test {
   protected:
    void SetUp() override {
        rate_limiter = std::make_unique<RateLimiter>();
    }

    void TearDown() override { rate_limiter.reset(); }

    std::unique_ptr<RateLimiter> rate_limiter;
};

// Test basic rate limiting for requests
TEST_F(RateLimiterTest, BasicRequestRateLimiting) {
    std::string client_id = "client123";

    // Should allow requests up to the limit (default: 120 per minute)
    for (int i = 0; i < 120; ++i) {
        EXPECT_TRUE(rate_limiter->is_request_allowed(client_id));
    }

    // Should block further requests after limit
    EXPECT_FALSE(rate_limiter->is_request_allowed(client_id));
    EXPECT_FALSE(rate_limiter->is_request_allowed(client_id));
}

// Test message rate limiting
TEST_F(RateLimiterTest, MessageRateLimiting) {
    std::string user_id = "user123";

    // Should allow messages up to the limit (default: 60 per minute)
    for (int i = 0; i < 60; ++i) {
        EXPECT_TRUE(rate_limiter->is_message_allowed(user_id));
    }

    // Should block further messages after limit
    EXPECT_FALSE(rate_limiter->is_message_allowed(user_id));
    EXPECT_FALSE(rate_limiter->is_message_allowed(user_id));
}

// Test connection rate limiting
TEST_F(RateLimiterTest, ConnectionRateLimiting) {
    std::string ip_address = "192.168.1.100";

    // Should allow connections up to the limit (default: 10 per minute)
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(rate_limiter->is_connection_allowed(ip_address));
    }

    // Should block further connections after limit
    EXPECT_FALSE(rate_limiter->is_connection_allowed(ip_address));
    EXPECT_FALSE(rate_limiter->is_connection_allowed(ip_address));
}

// Test rate limiting with different clients
TEST_F(RateLimiterTest, DifferentClients) {
    std::string client1 = "client1";
    std::string client2 = "client2";

    // Client 1 uses up their request limit
    for (int i = 0; i < 120; ++i) {
        EXPECT_TRUE(rate_limiter->is_request_allowed(client1));
    }
    EXPECT_FALSE(rate_limiter->is_request_allowed(client1));

    // Client 2 should still be allowed
    EXPECT_TRUE(rate_limiter->is_request_allowed(client2));
    
    // Different users for messages
    std::string user1 = "user1";
    std::string user2 = "user2";
    
    // User 1 uses up message limit
    for (int i = 0; i < 60; ++i) {
        EXPECT_TRUE(rate_limiter->is_message_allowed(user1));
    }
    EXPECT_FALSE(rate_limiter->is_message_allowed(user1));
    
    // User 2 should still be allowed
    EXPECT_TRUE(rate_limiter->is_message_allowed(user2));
}

// Test rate limit configuration
TEST_F(RateLimiterTest, RateLimitConfiguration) {
    std::string client_id = "config_client";
    std::string user_id = "config_user";
    std::string ip_address = "192.168.1.200";

    // Set custom rate limits
    rate_limiter->set_request_rate_limit(30);      // 30 requests per minute
    rate_limiter->set_message_rate_limit(20);      // 20 messages per minute
    rate_limiter->set_connection_rate_limit(5);    // 5 connections per minute

    // Test request rate limit
    for (int i = 0; i < 30; ++i) {
        EXPECT_TRUE(rate_limiter->is_request_allowed(client_id));
    }
    EXPECT_FALSE(rate_limiter->is_request_allowed(client_id));

    // Test message rate limit
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(rate_limiter->is_message_allowed(user_id));
    }
    EXPECT_FALSE(rate_limiter->is_message_allowed(user_id));

    // Test connection rate limit
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(rate_limiter->is_connection_allowed(ip_address));
    }
    EXPECT_FALSE(rate_limiter->is_connection_allowed(ip_address));
}

// Test blocking functionality
TEST_F(RateLimiterTest, BlockingFunctionality) {
    std::string client_id = "client_blocking";

    // Client should not be blocked initially
    EXPECT_FALSE(rate_limiter->is_client_blocked(client_id));

    // Block the client for 1 second
    rate_limiter->block_client(client_id, 1);
    EXPECT_TRUE(rate_limiter->is_client_blocked(client_id));

    // Blocked client should not be allowed
    EXPECT_FALSE(rate_limiter->is_request_allowed(client_id));

    // Wait for block to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Should be unblocked automatically
    EXPECT_FALSE(rate_limiter->is_client_blocked(client_id));
    EXPECT_TRUE(rate_limiter->is_request_allowed(client_id));
}

// Test manual unblocking
TEST_F(RateLimiterTest, ManualUnblocking) {
    std::string client_id = "client_manual_unblock";

    // Block the client
    rate_limiter->block_client(client_id, 300);  // 5 minutes
    EXPECT_TRUE(rate_limiter->is_client_blocked(client_id));
    EXPECT_FALSE(rate_limiter->is_request_allowed(client_id));

    // Manually unblock the client
    rate_limiter->unblock_client(client_id);
    EXPECT_FALSE(rate_limiter->is_client_blocked(client_id));
    EXPECT_TRUE(rate_limiter->is_request_allowed(client_id));
}

// Test cleanup functionality
TEST_F(RateLimiterTest, CleanupExpiredEntries) {
    std::string client1 = "client_cleanup1";
    std::string client2 = "client_cleanup2";

    // Make requests for both clients
    EXPECT_TRUE(rate_limiter->is_request_allowed(client1));
    EXPECT_TRUE(rate_limiter->is_request_allowed(client2));

    // Manually trigger cleanup
    rate_limiter->cleanup_expired_entries();

    // Should still be able to make requests
    EXPECT_TRUE(rate_limiter->is_request_allowed(client1));
    EXPECT_TRUE(rate_limiter->is_request_allowed(client2));
}

// Test reset rate limits
TEST_F(RateLimiterTest, ResetRateLimits) {
    std::string client_id = "reset_client";

    // Use up some requests
    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(rate_limiter->is_request_allowed(client_id));
    }

    // Reset rate limits for this client
    rate_limiter->reset_rate_limits(client_id);

    // Should be able to make requests again up to the full limit
    for (int i = 0; i < 120; ++i) {
        EXPECT_TRUE(rate_limiter->is_request_allowed(client_id));
    }
    EXPECT_FALSE(rate_limiter->is_request_allowed(client_id));
}

// Test edge cases
TEST_F(RateLimiterTest, EdgeCases) {
    // Empty client ID
    EXPECT_TRUE(rate_limiter->is_request_allowed(""));
    EXPECT_TRUE(rate_limiter->is_message_allowed(""));
    EXPECT_TRUE(rate_limiter->is_connection_allowed(""));

    // Very long client ID
    std::string long_client_id(1000, 'c');
    EXPECT_TRUE(rate_limiter->is_request_allowed(long_client_id));

    // Special characters in client ID
    std::string special_client = "client@#$%^&*()";
    EXPECT_TRUE(rate_limiter->is_request_allowed(special_client));

    // Unicode client ID
    std::string unicode_client = "客户端用户";
    EXPECT_TRUE(rate_limiter->is_request_allowed(unicode_client));

    // IP addresses
    EXPECT_TRUE(rate_limiter->is_connection_allowed("192.168.1.1"));
    EXPECT_TRUE(rate_limiter->is_connection_allowed("::1"));
    EXPECT_TRUE(rate_limiter->is_connection_allowed("invalid_ip"));
}

// Test thread safety
TEST_F(RateLimiterTest, ThreadSafety) {
    std::vector<std::thread> threads;
    std::atomic<int> allowed_count{0};
    std::atomic<int> blocked_count{0};
    std::string shared_client = "shared_client";

    // Multiple threads trying to access the same client
    for (int i = 0; i < 140; ++i) {  // More than the limit of 120
        threads.emplace_back([this, &allowed_count, &blocked_count, shared_client]() {
            if (rate_limiter->is_request_allowed(shared_client)) {
                allowed_count++;
            } else {
                blocked_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have exactly 120 allowed and 20 blocked
    EXPECT_EQ(allowed_count.load(), 120);
    EXPECT_EQ(blocked_count.load(), 20);
}

// Test concurrent access with different clients
TEST_F(RateLimiterTest, ConcurrentDifferentClients) {
    std::vector<std::thread> threads;
    std::atomic<int> total_allowed{0};

    // Each thread uses a different client
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &total_allowed]() {
            std::string client_id = "client_" + std::to_string(i);
            int client_allowed = 0;
            
            // Each client should be able to make up to 120 requests
            for (int j = 0; j < 130; ++j) {  // Try more than limit
                if (rate_limiter->is_request_allowed(client_id)) {
                    client_allowed++;
                }
            }
            
            total_allowed += client_allowed;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have 10 clients * 120 allowed requests each = 1200 total
    EXPECT_EQ(total_allowed.load(), 1200);
}

// Test performance under load
TEST_F(RateLimiterTest, PerformanceUnderLoad) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Make many requests quickly
    for (int i = 0; i < 1000; ++i) {
        std::string client_id = "perf_client_" + std::to_string(i % 100);  // 100 different clients
        rate_limiter->is_request_allowed(client_id);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000);  // Less than 1 second for 1000 operations
}
