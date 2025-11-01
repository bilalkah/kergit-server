#include "utils/Loggable.h"

#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

class Dummy : public utils::Loggable {
   public:
    void say() const { log(utils::LogLevel::INFO, "x=", 42, " ok"); }
};

TEST(Loggable, PrefixesClassNameAndConcats) {
    std::ostringstream capture;
    auto* old = std::cout.rdbuf(capture.rdbuf());

    Dummy d;
    d.say();

    std::cout.rdbuf(old);
    std::string out = capture.str();

    // Contains INFO and concatenated content
    EXPECT_NE(out.find("[INFO]"), std::string::npos);
    EXPECT_NE(out.find("x=42 ok"), std::string::npos);

    // Contains class name (demangled if available); just check "Dummy" substring
    EXPECT_NE(out.find("Dummy:"), std::string::npos);
}