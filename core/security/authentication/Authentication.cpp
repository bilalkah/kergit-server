#include "core/security/authentication/Authentication.h"

#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

// Simple hash function for demonstration (replace with proper crypto in production)
std::size_t simple_hash(const std::string& str) {
    std::hash<std::string> hasher;
    return hasher(str);
}

Authentication::Authentication() {
    // Initialize with some default admin user for testing
    register_user("admin", "admin@example.com", "admin123");
    register_user("user1", "user1@example.com", "password123");
    register_user("test", "test@example.com", "test123");
}

Authentication::~Authentication() = default;

bool Authentication::register_user(const std::string& username, const std::string& email,
                                   const std::string& password) {
    // Check if user already exists
    if (username_to_user_.find(username) != username_to_user_.end()) {
        return false;  // User already exists
    }

    if (password.length() < MIN_PASSWORD_LENGTH) {
        return false;  // Password too short
    }

    // Generate salt and hash password
    std::string salt = generate_salt();
    std::string password_hash = hash_password(password, salt);

    // Create new user
    auto user = std::make_shared<AuthUser>();
    user->id = std::to_string(simple_hash(username + email + std::to_string(std::time(nullptr))));
    user->username = username;
    user->email = email;
    user->password_hash = password_hash;
    user->salt = salt;
    user->created_at = std::chrono::system_clock::now();
    user->is_active = true;
    user->role = "user";

    // Store user
    users_[user->id] = user;
    username_to_user_[username] = user;

    std::cout << "[AUTH] User registered: " << username << " (ID: " << user->id << ")" << std::endl;
    return true;
}

bool Authentication::authenticate_user(const std::string& username, const std::string& password) {
    auto it = username_to_user_.find(username);
    if (it == username_to_user_.end()) {
        std::cout << "[AUTH] User not found: " << username << std::endl;
        return false;  // User not found
    }

    auto user = it->second;
    if (!user->is_active) {
        std::cout << "[AUTH] Account disabled: " << username << std::endl;
        return false;  // Account disabled
    }

    // Verify password
    bool valid = verify_password(password, user->password_hash, user->salt);
    if (valid) {
        user->last_login = std::chrono::system_clock::now();
        std::cout << "[AUTH] User authenticated: " << username << std::endl;
    } else {
        std::cout << "[AUTH] Invalid password for user: " << username << std::endl;
    }

    return valid;
}

std::string Authentication::hash_password(const std::string& password, const std::string& salt) {
    // Simple hash implementation (use bcrypt/scrypt in production)
    std::string salted_password = password + salt;
    std::size_t hash_value = simple_hash(salted_password);

    std::stringstream ss;
    ss << std::hex << hash_value;
    return ss.str();
}

std::string Authentication::generate_salt() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::stringstream ss;
    for (int i = 0; i < 16; i++) {  // Reduced salt length for simplicity
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }

    return ss.str();
}

bool Authentication::verify_password(const std::string& password, const std::string& hash,
                                     const std::string& salt) {
    std::string computed_hash = hash_password(password, salt);
    return computed_hash == hash;
}

std::shared_ptr<AuthUser> Authentication::get_user(const std::string& user_id) {
    auto it = users_.find(user_id);
    return (it != users_.end()) ? it->second : nullptr;
}

std::shared_ptr<AuthUser> Authentication::get_user_by_username(const std::string& username) {
    auto it = username_to_user_.find(username);
    return (it != username_to_user_.end()) ? it->second : nullptr;
}

bool Authentication::update_last_login(const std::string& user_id) {
    auto user = get_user(user_id);
    if (user) {
        user->last_login = std::chrono::system_clock::now();
        return true;
    }
    return false;
}

bool Authentication::is_session_valid(const std::string& session_id) {
    // Simple session validation - check if session exists and is not expired
    return !session_id.empty() && session_id.length() > 10;
}

std::shared_ptr<SessionInfo> Authentication::get_session(const std::string& session_id) {
    // Stub implementation - would normally fetch from session store
    if (session_id.empty()) {
        return nullptr;
    }

    // Create a mock session for testing
    auto session = std::make_shared<SessionInfo>();
    session->session_id = session_id;
    session->created_at = std::chrono::system_clock::now();
    session->expires_at = std::chrono::system_clock::now() + std::chrono::hours(24);
    session->last_activity = std::chrono::system_clock::now();
    session->is_valid = true;

    return session;
}
