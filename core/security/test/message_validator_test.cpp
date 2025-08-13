#include "core/security/message_validator/MessageValidator.h"

#include <gtest/gtest.h>
#include <memory>

class MessageValidatorTest : public ::testing::Test {
   protected:
    void SetUp() override { validator = std::make_unique<MessageValidator>(); }

    void TearDown() override { validator.reset(); }

    std::unique_ptr<MessageValidator> validator;

    json create_test_message(const std::string& type = "chat",
                             const std::string& text = "Hello world",
                             const std::string& username = "testuser",
                             const std::string& channel = "general") {
        return json{{"type", type},
                    {"id", "msg_" + std::to_string(std::time(nullptr))},
                    {"timestamp", std::time(nullptr)},
                    {"text", text},
                    {"username", username},
                    {"channel", channel}};
    }
};

// Test valid message validation
TEST_F(MessageValidatorTest, ValidMessage) {
    json message = create_test_message();

    MessageValidationResult result = validator->validate_message(message);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.message_type, MessageType::CHAT);
    EXPECT_TRUE(result.error_message.empty());
}

// Test message format validation
TEST_F(MessageValidatorTest, MessageFormatValidation) {
    // Invalid JSON (not an object)
    json invalid_array = json::array({1, 2, 3});
    EXPECT_FALSE(validator->is_message_format_valid(invalid_array));

    // Missing required fields
    json missing_type = {{"id", "123"}, {"timestamp", 123456}};
    EXPECT_FALSE(validator->is_message_format_valid(missing_type));

    json missing_id = {{"type", "chat"}, {"timestamp", 123456}};
    EXPECT_FALSE(validator->is_message_format_valid(missing_id));

    json missing_timestamp = {{"type", "chat"}, {"id", "123"}};
    EXPECT_FALSE(validator->is_message_format_valid(missing_timestamp));

    // Valid format
    json valid_message = {{"type", "chat"}, {"id", "123"}, {"timestamp", 123456}};
    EXPECT_TRUE(validator->is_message_format_valid(valid_message));
}

// Test message size validation
TEST_F(MessageValidatorTest, MessageSizeValidation) {
    // Small message should be valid
    json small_message = create_test_message();
    EXPECT_TRUE(validator->is_message_size_valid(small_message));

    // Large message should be invalid
    std::string large_text(5000, 'a');  // Larger than MAX_MESSAGE_SIZE
    json large_message = create_test_message("chat", large_text);
    EXPECT_FALSE(validator->is_message_size_valid(large_message));
}

// Test content safety validation
TEST_F(MessageValidatorTest, ContentSafetyValidation) {
    // Safe content
    json safe_message = create_test_message();
    EXPECT_TRUE(validator->is_message_content_safe(safe_message));

    // Content with profanity
    json profanity_message = create_test_message("chat", "This is spam content");
    EXPECT_FALSE(validator->is_message_content_safe(profanity_message));

    // Content with malicious patterns
    json malicious_message = create_test_message("chat", "eval(malicious_code)");
    EXPECT_FALSE(validator->is_message_content_safe(malicious_message));
}

// Test profanity detection
TEST_F(MessageValidatorTest, ProfanityDetection) {
    EXPECT_TRUE(validator->contains_profanity("This is spam"));
    EXPECT_TRUE(validator->contains_profanity("SPAM content here"));
    EXPECT_FALSE(validator->contains_profanity("This is a normal message"));
    EXPECT_FALSE(validator->contains_profanity(""));
}

// Test malicious content detection
TEST_F(MessageValidatorTest, MaliciousContentDetection) {
    EXPECT_TRUE(validator->contains_malicious_content("eval(alert('xss'))"));
    EXPECT_TRUE(validator->contains_malicious_content("system('rm -rf /')"));
    EXPECT_FALSE(validator->contains_malicious_content("Hello world"));
    EXPECT_FALSE(validator->contains_malicious_content(""));
}

// Test SQL injection detection
TEST_F(MessageValidatorTest, SQLInjectionDetection) {
    EXPECT_TRUE(validator->is_sql_injection_attempt("'; DROP TABLE users; --"));
    EXPECT_TRUE(validator->is_sql_injection_attempt("1 UNION SELECT * FROM passwords"));
    EXPECT_TRUE(validator->is_sql_injection_attempt("admin'--"));
    EXPECT_FALSE(validator->is_sql_injection_attempt("Hello world"));
    EXPECT_FALSE(validator->is_sql_injection_attempt(""));
}

// Test XSS detection
TEST_F(MessageValidatorTest, XSSDetection) {
    EXPECT_TRUE(validator->is_xss_attempt("<script>alert('xss')</script>"));
    EXPECT_TRUE(validator->is_xss_attempt("javascript:alert('xss')"));
    EXPECT_TRUE(validator->is_xss_attempt("<img onload='alert(1)'>"));
    EXPECT_FALSE(validator->is_xss_attempt("Hello world"));
    EXPECT_FALSE(validator->is_xss_attempt(""));
}

// Test message type detection
TEST_F(MessageValidatorTest, MessageTypeDetection) {
    EXPECT_EQ(validator->get_message_type(json{{"type", "chat"}}), MessageType::CHAT);
    EXPECT_EQ(validator->get_message_type(json{{"type", "join"}}), MessageType::JOIN_CHANNEL);
    EXPECT_EQ(validator->get_message_type(json{{"type", "leave"}}), MessageType::LEAVE_CHANNEL);
    EXPECT_EQ(validator->get_message_type(json{{"type", "ping"}}), MessageType::PING);
    EXPECT_EQ(validator->get_message_type(json{{"type", "unknown"}}), MessageType::UNKNOWN);
    EXPECT_EQ(validator->get_message_type(json{{"no_type", "test"}}), MessageType::UNKNOWN);
}

// Test chat message validation
TEST_F(MessageValidatorTest, ChatMessageValidation) {
    // Valid chat message
    json valid_chat = create_test_message();
    EXPECT_TRUE(validator->validate_chat_message(valid_chat));

    // Missing text field
    json no_text = create_test_message();
    no_text.erase("text");
    EXPECT_FALSE(validator->validate_chat_message(no_text));

    // Missing username field
    json no_username = create_test_message();
    no_username.erase("username");
    EXPECT_FALSE(validator->validate_chat_message(no_username));

    // Missing channel field
    json no_channel = create_test_message();
    no_channel.erase("channel");
    EXPECT_FALSE(validator->validate_chat_message(no_channel));

    // Text too long
    std::string long_text(3000, 'a');  // Longer than MAX_TEXT_LENGTH
    json long_message = create_test_message("chat", long_text);
    EXPECT_FALSE(validator->validate_chat_message(long_message));
}

// Test join message validation
TEST_F(MessageValidatorTest, JoinMessageValidation) {
    json join_message = {{"type", "join"},
                         {"id", "join_123"},
                         {"timestamp", 123456},
                         {"username", "testuser"},
                         {"channel", "general"}};

    EXPECT_TRUE(validator->validate_join_message(join_message));

    // Missing username
    json no_username = join_message;
    no_username.erase("username");
    EXPECT_FALSE(validator->validate_join_message(no_username));

    // Missing channel
    json no_channel = join_message;
    no_channel.erase("channel");
    EXPECT_FALSE(validator->validate_join_message(no_channel));
}

// Test channel name validation
TEST_F(MessageValidatorTest, ChannelNameValidation) {
    EXPECT_TRUE(validator->is_channel_name_valid("general"));
    EXPECT_TRUE(validator->is_channel_name_valid("test-channel"));
    EXPECT_TRUE(validator->is_channel_name_valid("channel_123"));
    EXPECT_TRUE(validator->is_channel_name_valid("ABC123"));

    EXPECT_FALSE(validator->is_channel_name_valid(""));                     // Empty
    EXPECT_FALSE(validator->is_channel_name_valid("channel with spaces"));  // Spaces
    EXPECT_FALSE(validator->is_channel_name_valid("channel@special"));      // Special chars

    // Too long
    std::string long_channel(100, 'a');
    EXPECT_FALSE(validator->is_channel_name_valid(long_channel));
}

// Test username validation
TEST_F(MessageValidatorTest, UsernameValidation) {
    EXPECT_TRUE(validator->is_username_valid("testuser"));
    EXPECT_TRUE(validator->is_username_valid("user123"));
    EXPECT_TRUE(validator->is_username_valid("test_user"));
    EXPECT_TRUE(validator->is_username_valid("ABC"));

    EXPECT_FALSE(validator->is_username_valid(""));                  // Empty
    EXPECT_FALSE(validator->is_username_valid("user with spaces"));  // Spaces
    EXPECT_FALSE(validator->is_username_valid("user@domain"));       // Special chars
    EXPECT_FALSE(validator->is_username_valid("user-name"));         // Dash not allowed

    // Too long
    std::string long_username(50, 'a');
    EXPECT_FALSE(validator->is_username_valid(long_username));
}

// Test message ID uniqueness
TEST_F(MessageValidatorTest, MessageIdUniqueness) {
    std::string test_id = "unique_test_id";

    // First time should be unique
    EXPECT_TRUE(validator->is_message_id_unique(test_id));

    // Second time should not be unique
    EXPECT_FALSE(validator->is_message_id_unique(test_id));

    // Different ID should be unique
    EXPECT_TRUE(validator->is_message_id_unique("another_unique_id"));
}

// Test complete message validation workflow
TEST_F(MessageValidatorTest, CompleteValidationWorkflow) {
    // Valid message should pass all checks
    json valid_message = create_test_message();
    MessageValidationResult result = validator->validate_message(valid_message);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.message_type, MessageType::CHAT);

    // Invalid format should fail
    json invalid_format = json::array({1, 2, 3});
    result = validator->validate_message(invalid_format);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.error_message, "Invalid message format");

    // Too large message should fail
    std::string large_text(5000, 'a');
    json large_message = create_test_message("chat", large_text);
    result = validator->validate_message(large_message);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.error_message, "Message too large");

    // Unsafe content should fail
    json unsafe_message = create_test_message("chat", "spam content here");
    result = validator->validate_message(unsafe_message);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.error_message, "Message contains unsafe content");
}

// Test edge cases
TEST_F(MessageValidatorTest, EdgeCases) {
    // Empty strings
    EXPECT_FALSE(validator->contains_profanity(""));
    EXPECT_FALSE(validator->contains_malicious_content(""));
    EXPECT_FALSE(validator->is_sql_injection_attempt(""));
    EXPECT_FALSE(validator->is_xss_attempt(""));

    // Very long safe content
    std::string long_safe_text(1000, 'a');
    EXPECT_FALSE(validator->contains_profanity(long_safe_text));
    EXPECT_FALSE(validator->contains_malicious_content(long_safe_text));

    // Case sensitivity
    EXPECT_TRUE(validator->contains_profanity("SPAM"));
    EXPECT_TRUE(validator->contains_profanity("Spam"));
    EXPECT_TRUE(validator->contains_profanity("spam"));

    // Unicode content
    std::string unicode_text = "Hello 世界 🌍";
    EXPECT_FALSE(validator->contains_profanity(unicode_text));
    EXPECT_FALSE(validator->contains_malicious_content(unicode_text));
}
