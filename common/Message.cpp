#include "Message.h"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

Message::Message() {
    generate_message_id();
    timestamp = std::chrono::system_clock::now();
    security_info.timestamp = timestamp;
}

Message::Message(MessageType msg_type, const std::string& content, const std::string& sender_name)
    : type(msg_type), sender(sender_name), text(content) {
    generate_message_id();
    timestamp = std::chrono::system_clock::now();
    security_info.timestamp = timestamp;
}

void Message::generate_message_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 16; i++) {
        ss << std::setw(2) << std::setfill('0') << dis(gen);
    }
    id = ss.str();
    security_info.message_id = id;
}

void Message::sign_message(const std::string& secret_key) {
    // Simple HMAC-like signature (use proper HMAC in production)
    std::string message_data = id + text + sender + std::to_string(sequence_number);
    std::hash<std::string> hasher;
    size_t hash_value = hasher(message_data + secret_key);

    std::stringstream ss;
    ss << std::hex << hash_value;
    security_info.hmac_signature = ss.str();
    security_info.integrity_verified = true;
}

bool Message::verify_signature(const std::string& secret_key) const {
    // Verify the HMAC signature
    std::string message_data = id + text + sender + std::to_string(sequence_number);
    std::hash<std::string> hasher;
    size_t hash_value = hasher(message_data + secret_key);

    std::stringstream ss;
    ss << std::hex << hash_value;
    std::string computed_signature = ss.str();

    return computed_signature == security_info.hmac_signature;
}

bool Message::is_integrity_valid() const {
    return security_info.integrity_verified && !security_info.hmac_signature.empty();
}

bool Message::is_chat_message() const { return type == MessageType::CHAT; }

bool Message::is_system_message() const { return type == MessageType::SYSTEM; }

bool Message::requires_authentication() const {
    // All message types except ping require authentication
    return type != MessageType::PING && type != MessageType::PONG;
}

std::string Message::get_display_text() const {
    if (is_deleted) {
        return "[Message deleted]";
    }
    return text;
}

bool Message::is_valid() const { return validate_content() && validate_security_info(); }

bool Message::validate_content() const {
    // Basic content validation
    if (id.empty() || sender.empty()) {
        return false;
    }

    // Check message size limits
    if (text.length() > 2048) {  // 2KB limit
        return false;
    }

    return true;
}

bool Message::validate_security_info() const {
    // Validate security information
    if (security_info.message_id != id) {
        return false;
    }

    // Check timestamp is reasonable (within last hour to 1 minute in future)
    auto now = std::chrono::system_clock::now();
    auto time_diff =
        std::chrono::duration_cast<std::chrono::minutes>(now - security_info.timestamp);

    if (time_diff.count() > 60 || time_diff.count() < -1) {
        return false;
    }

    return true;
}
