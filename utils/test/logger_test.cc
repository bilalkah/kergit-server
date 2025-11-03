#include "utils/Logger.h"
#include <gtest/gtest.h>
#include <sstream>
#include <iostream>

using utils::LogLevel;
using utils::log_line;

TEST(Logger, PrintsLevelAndMessage) {
    std::ostringstream capture;
    auto* old = std::cout.rdbuf(capture.rdbuf());

    log_line(LogLevel::INFO, "hello");
    std::cout.rdbuf(old);

    std::string out = capture.str();
    EXPECT_NE(out.find("[INFO]"), std::string::npos);
    EXPECT_NE(out.find("hello"), std::string::npos);
}

TEST(Logger, DifferentLevels) {
    std::ostringstream capture;
    auto* old = std::cout.rdbuf(capture.rdbuf());

    log_line(LogLevel::WARN, "warn");
    log_line(LogLevel::ERROR, "crit");
    std::cout.rdbuf(old);

    std::string out = capture.str();
    EXPECT_NE(out.find("[WARN]"), std::string::npos);
    EXPECT_NE(out.find("[CRIT]"), std::string::npos);
}

