#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

enum class MessageType {
    CHAT,
    SYSTEM,
    JOIN_CHANNEL,
    LEAVE_CHANNEL,
    USER_LIST,
    PING,
    PONG,
    CALL_END,
    USER_STATE_CHANGE,
    USER_TYPING,
    USER_STOP_TYPING,
    UNKNOWN
};

enum class MessageStatus { PENDING, SENT, DELIVERED, READ, FAILED, BLOCKED };

struct MessageSecurity {
    std::string message_id;
    std::string hmac_signature;
    std::string session_id;
    std::chrono::system_clock::time_point timestamp;
    std::string client_ip;
    bool is_encrypted = false;
    bool integrity_verified = false;
};

class Message {
   public:
    // Message identification
    std::string id;
    std::string conversation_id;
    std::string thread_id;

    // Message content
    MessageType type = MessageType::CHAT;
    std::string sender;
    std::string text;
    std::string sender_id;
    std::string sender_username;
    std::string channel;
    std::string target_user;
    std::string target_user_id;  // For private messages or calls

    // Message metadata
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point edited_at;
    MessageStatus status = MessageStatus::PENDING;
    int sequence_number = 0;

    // Security and validation
    MessageSecurity security_info;
    std::unordered_map<std::string, std::string> headers;

    // Message features
    bool is_edited = false;
    bool is_deleted = false;
    bool is_pinned = false;
    std::string reply_to_message_id;
    std::vector<std::string> attachments;

    // Constructors
    Message();
    Message(MessageType msg_type, const std::string& content, const std::string& sender_name);

    // Security methods
    void sign_message(const std::string& secret_key);
    bool verify_signature(const std::string& secret_key) const;
    void encrypt_content(const std::string& encryption_key);
    bool decrypt_content(const std::string& encryption_key);
    bool is_integrity_valid() const;

    // Utility methods
    std::string to_json() const;
    bool from_json(const std::string& json_str);
    std::string get_display_text() const;
    bool is_valid() const;

    bool is_chat_message() const;
    bool is_system_message() const;
    bool requires_authentication() const;

   private:
    // Generate unique message ID
    void generate_message_id();

    // Validation helpers
    bool validate_content() const;
    bool validate_security_info() const;
};