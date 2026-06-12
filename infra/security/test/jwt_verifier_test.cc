#include "infra/security/token/SupabaseVerifier.h"
#include "utils/EnvLoader.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <gtest/gtest.h>
using infra::security::token::SupabaseVerifier;
using utils::EnvLoader;

TEST(JwtVerifier, MissingKeysReturnError) {
    EnvLoader::clear_env();
    EnvLoader::set_env("SUPABASE_JWT_CURRENT_KEY", "");
    EnvLoader::set_env("SUPABASE_JWT_STANDBY_KEY", "");

    auto verifier_exp = SupabaseVerifier::create();
    EXPECT_FALSE(verifier_exp.has_value());
    if (!verifier_exp.has_value()) {
        EXPECT_EQ(verifier_exp.error(), infra::security::token::JwtVerifyError::KeyNotFound);
    }

    EnvLoader::clear_env();
}

TEST(JwtVerifier, EmptyTokenReturnsInvalidFormat) {
    EnvLoader::load_env_file();
    auto verifier_exp = SupabaseVerifier::create();
    if (!verifier_exp.has_value()) {
        GTEST_SKIP() << "Supabase JWKs not configured for tests";
    }

    auto res = verifier_exp->verify_token("");
    EXPECT_FALSE(res.has_value());
    if (!res.has_value()) {
        EXPECT_EQ(res.error(), infra::security::token::JwtVerifyError::EmptyToken);
    }
    EnvLoader::clear_env();
}

TEST(JwtVerifier, GarbageTokenReturnsInvalidFormat) {
    EnvLoader::load_env_file();
    auto verifier_exp = SupabaseVerifier::create();
    if (!verifier_exp.has_value()) {
        GTEST_SKIP() << "Supabase JWKs not configured for tests";
    }

    auto res = verifier_exp->verify_token("not.a.jwt");
    EXPECT_FALSE(res.has_value());
    if (!res.has_value()) {
        EXPECT_EQ(res.error(), infra::security::token::JwtVerifyError::InvalidFormat);
    }
    EnvLoader::clear_env();
}

TEST(JwtVerifier, BenchmarkSampleTokens) {
    EnvLoader::load_env_file();
    auto verifier_exp = SupabaseVerifier::create();
    if (!verifier_exp.has_value()) {
        GTEST_SKIP() << "Supabase JWKs not configured for tests";
    }

    std::ifstream file("infra/security/test/test_token.sample");
    if (!file.is_open()) {
        GTEST_SKIP() << "JWT sample file not found: infra/security/test/test_token.sample";
    }

    std::vector<std::string> tokens;
    std::string line;
    while (std::getline(file, line)) {
        const auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        if (line[start] == '#') continue;
        const auto end = line.find_last_not_of(" \t\r\n");
        tokens.push_back(line.substr(start, end - start + 1));
    }
    if (tokens.empty()) {
        GTEST_SKIP() << "No JWTs found in sample file";
    }

    constexpr int kIterations = 5000;
    const uint64_t total_verifications =
        static_cast<uint64_t>(tokens.size()) * static_cast<uint64_t>(kIterations);
    long long min_us = std::numeric_limits<long long>::max();
    long long max_us = 0;
    long long sum_us = 0;

    auto total_start = std::chrono::steady_clock::now();
    for (const auto& token : tokens) {
        for (int i = 0; i < kIterations; ++i) {
            auto start_time = std::chrono::steady_clock::now();
            (void)verifier_exp->verify_token(token);
            auto end_time = std::chrono::steady_clock::now();
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time)
                          .count();
            if (us < min_us) min_us = us;
            if (us > max_us) max_us = us;
            sum_us += us;
        }
    }
    auto total_end = std::chrono::steady_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start)
                        .count();
    const double avg_us = total_verifications > 0
                              ? static_cast<double>(sum_us) /
                                    static_cast<double>(total_verifications)
                              : 0.0;

    std::cout << "JWT verification benchmark\n\n";
    std::cout << "tokens loaded: " << tokens.size() << "\n";
    std::cout << "iterations per token: " << kIterations << "\n";
    std::cout << "total verifications: " << total_verifications << "\n\n";
    std::cout << "avg verify time (us): " << avg_us << "\n";
    std::cout << "min verify time (us): " << min_us << "\n";
    std::cout << "max verify time (us): " << max_us << "\n";
    std::cout << "total time (ms): " << total_ms << "\n";

    EnvLoader::clear_env();
}
