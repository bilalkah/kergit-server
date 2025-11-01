#include "infra/security/token/SupabaseJWTVerifier.h"
#include "utils/EnvLoader.h"

#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

using infra::security::token::SupabaseJWTVerifier;

class CoutCapture {
   public:
    CoutCapture() : old_(std::cout.rdbuf(buf_.rdbuf())) {}
    ~CoutCapture() { std::cout.rdbuf(old_); }
    std::string str() const { return buf_.str(); }

   private:
    std::ostringstream buf_;
    std::streambuf* old_;
};

TEST(JwtVerifier, EmptyTokenReturnsNullAndWarns) {
    // Ensure env keys are empty for this test; your EnvLoader keeps its own map.
    EnvLoader::set_env("SUPABASE_JWT_CURRENT_KEY", "");
    EnvLoader::set_env("SUPABASE_JWT_STANDBY_KEY", "");

    SupabaseJWTVerifier verifier;  // uses EnvLoader internally

    CoutCapture cap;
    auto res = verifier.verify_token("");  // empty token
    std::string out = cap.str();

    EXPECT_FALSE(res.has_value());
    // We only check that something warning-like is present; exact text is brittle.
    EXPECT_TRUE(out.find("WARN") != std::string::npos || out.find("WARNING") != std::string::npos);
}

TEST(JwtVerifier, GarbageTokenReturnsNull) {
    SupabaseJWTVerifier verifier;

    CoutCapture cap;
    auto res = verifier.verify_token("not.a.jwt");
    std::string out = cap.str();

    EXPECT_FALSE(res.has_value());
    EXPECT_TRUE(out.find("WARN") != std::string::npos || out.find("WARNING") != std::string::npos);
}
