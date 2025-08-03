#pragma once
#include <memory>
#include <string>
#include <vector>

class MessageEncryption {
   public:
    MessageEncryption();
    ~MessageEncryption();

    // Encryption/Decryption
    std::string encrypt_message(const std::string& plaintext, const std::string& key);
    std::string decrypt_message(const std::string& ciphertext, const std::string& key);

    // Key management
    std::string generate_encryption_key();
    std::string derive_key_from_password(const std::string& password, const std::string& salt);

    // Secure data handling
    bool secure_compare(const std::string& a, const std::string& b);
    void secure_zero_memory(std::string& data);

   private:
    static constexpr int AES_KEY_SIZE = 32;  // 256 bits
    static constexpr int AES_IV_SIZE = 16;   // 128 bits
    static constexpr int PBKDF2_ITERATIONS = 100000;

    // Helper methods
    std::vector<unsigned char> generate_random_bytes(int length);
    std::string bytes_to_hex(const std::vector<unsigned char>& bytes);
    std::vector<unsigned char> hex_to_bytes(const std::string& hex);
};
