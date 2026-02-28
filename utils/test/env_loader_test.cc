#include "utils/EnvLoader.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;
using utils::EnvLoader;

namespace {

struct TempEnvFile {
    fs::path path;
    explicit TempEnvFile(const std::string& filename_hint = "envloader_test.env") {
        auto base = fs::temp_directory_path() / filename_hint;
#if defined(_WIN32)
        path = base;
#else
        path = base;
        path += "." + std::to_string(::getpid());
#endif
    }
    ~TempEnvFile() {
        std::error_code ec;
        fs::remove(path, ec);
    }
};


struct EnvLoaderTest : ::testing::Test {
    void SetUp() override { EnvLoader::clear_env(); }
    void TearDown() override { EnvLoader::clear_env(); }
};

}  // namespace

TEST_F(EnvLoaderTest, LoadMissingFileReturnsFalse) {
    EXPECT_FALSE(EnvLoader::load_env_file("/path/that/does/not/exist/.env"));
}

TEST_F(EnvLoaderTest, SimpleLoadAndGet) {
    TempEnvFile t;
    {
        std::ofstream out(t.path);
        out << "FOO=bar\n";
        out << "NUM=42\n";
    }

    EXPECT_TRUE(EnvLoader::load_env_file(t.path.string()));
    EXPECT_EQ(EnvLoader::get_env("FOO"), "bar");
    EXPECT_EQ(EnvLoader::get_env("NUM"), "42");
    EXPECT_EQ(EnvLoader::get_env("MISSING", "default"), "default");
}

TEST_F(EnvLoaderTest, IgnoresCommentsAndBlankLines) {
    TempEnvFile t;
    {
        std::ofstream out(t.path);
        out << "# comment line\n";
        out << "\n";
        out << "A=1\n";
        out << "  \n";
        out << "# another comment\n";
        out << "B=2\n";
    }

    EXPECT_TRUE(EnvLoader::load_env_file(t.path.string()));
    EXPECT_EQ(EnvLoader::get_env("A"), "1");
    EXPECT_EQ(EnvLoader::get_env("B"), "2");
    EXPECT_EQ(EnvLoader::get_env("C", "x"), "x");
}

TEST_F(EnvLoaderTest, InvalidLinesAreIgnored) {
    TempEnvFile t;
    {
        std::ofstream out(t.path);
        out << "VALID=ok\n";
        out << "INVALID_LINE_WITHOUT_EQUALS\n";
        out << "ANOTHER_VALID=yes\n";
    }

    EXPECT_TRUE(EnvLoader::load_env_file(t.path.string()));
    EXPECT_EQ(EnvLoader::get_env("VALID"), "ok");
    EXPECT_EQ(EnvLoader::get_env("ANOTHER_VALID"), "yes");
    EXPECT_EQ(EnvLoader::get_env("INVALID_LINE_WITHOUT_EQUALS", "no"), "no");
}

TEST_F(EnvLoaderTest, LaterDuplicateOverridesEarlier) {
    TempEnvFile t;
    {
        std::ofstream out(t.path);
        out << "DUP=value1\n";
        out << "DUP=value2\n";
    }

    EXPECT_TRUE(EnvLoader::load_env_file(t.path.string()));
    EXPECT_EQ(EnvLoader::get_env("DUP"), "value2");
}

TEST_F(EnvLoaderTest, ValuesMayContainEqualsAfterFirstOne) {
    TempEnvFile t;
    {
        std::ofstream out(t.path);
        out << "WITH_EQ=a=b=c\n";
    }
    EXPECT_TRUE(EnvLoader::load_env_file(t.path.string()));
    EXPECT_EQ(EnvLoader::get_env("WITH_EQ"), "a=b=c");
}

// === New: load the completely fictional sample.env from test data ===
TEST_F(EnvLoaderTest, LoadFantasySampleEnvFromRunfiles) {
    const fs::path sample{"utils/test/sample.env"};
    ASSERT_TRUE(fs::exists(sample)) << "sample.env not found at: " << sample;

    EXPECT_TRUE(EnvLoader::load_env_file(sample.string()));

    // Straightforward keys
    EXPECT_EQ(EnvLoader::get_env("WORLD_NAME"), "GlitterPond");
    EXPECT_EQ(EnvLoader::get_env("CREATURE"), "sky-squid");
    EXPECT_EQ(EnvLoader::get_env("LEVEL"), "42");

    // Multiple '=' retained in value
    EXPECT_EQ(EnvLoader::get_env("PORTAL_COORDS"), "alpha=don't-care=omega'");
    EXPECT_NE(EnvLoader::get_env("LORE_TEXT").find("==in a land"), std::string::npos);

    // Keys with spaces likely won't be mapped unless you trim keys in parser
    EXPECT_EQ(EnvLoader::get_env("STRANGE KEY", "default"), "why_not_take_a_break");

    // Malformed lines should be ignored
    EXPECT_EQ(EnvLoader::get_env("NO_EQUALS_HERE", "nope"), "nope");
    EXPECT_EQ(EnvLoader::get_env("JUST_EQUALS_FIRST", "nope"), "nope");
}

// === New: ad-hoc “fantasy” content written by the test ===
TEST_F(EnvLoaderTest, AdHocFantasyEnvInline) {
    TempEnvFile t;
    {
        std::ofstream out(t.path);
        out << "APP_NAME=EchoRealm\n";
        out << "DEBUG_MODE=true\n";
        out << "CUSTOM_MESSAGE=Hello=World=Tester\n";
        out << "# comment\n";
        out << "NO_EQUALS_LINE\n";
    }

    EXPECT_TRUE(EnvLoader::load_env_file(t.path.string()));
    EXPECT_EQ(EnvLoader::get_env("APP_NAME"), "EchoRealm");
    EXPECT_EQ(EnvLoader::get_env("DEBUG_MODE"), "true");
    EXPECT_EQ(EnvLoader::get_env("CUSTOM_MESSAGE"), "Hello=World=Tester");
    EXPECT_EQ(EnvLoader::get_env("NO_EQUALS_LINE", "ignored"), "ignored");
}

TEST_F(EnvLoaderTest, SetEnvAndClearEnvWork) {
    EXPECT_EQ(EnvLoader::get_env("X", ""), "");
    EnvLoader::set_env("X", "123");
    EnvLoader::set_env("Y", "456");
    EXPECT_EQ(EnvLoader::get_env("X"), "123");
    EXPECT_EQ(EnvLoader::get_env("Y"), "456");
    EnvLoader::clear_env();
    EXPECT_EQ(EnvLoader::get_env("X", "d"), "d");
    EXPECT_EQ(EnvLoader::get_env("Y", "d"), "d");
}

TEST_F(EnvLoaderTest, TypedGetParsesSupportedScalarTypes) {
    EnvLoader::set_env("INT_VALUE", "42");
    EnvLoader::set_env("SIZE_VALUE", "2048");
    EnvLoader::set_env("BOOL_TRUE", "on");
    EnvLoader::set_env("BOOL_FALSE", "false");
    EnvLoader::set_env("FLOAT_VALUE", "3.5");

    EXPECT_EQ(EnvLoader::get<int>("INT_VALUE", 0), 42);
    EXPECT_EQ(EnvLoader::get<std::size_t>("SIZE_VALUE", 0), 2048U);
    EXPECT_TRUE(EnvLoader::get<bool>("BOOL_TRUE", false));
    EXPECT_FALSE(EnvLoader::get<bool>("BOOL_FALSE", true));
    EXPECT_FLOAT_EQ(EnvLoader::get<float>("FLOAT_VALUE", 0.0f), 3.5f);
    EXPECT_EQ(EnvLoader::get<int>("MISSING_INT", 7), 7);
}

TEST_F(EnvLoaderTest, TypedGetThrowsOnInvalidScalarValues) {
    EnvLoader::set_env("BAD_INT", "12x");
    EnvLoader::set_env("BAD_BOOL", "maybe");
    EnvLoader::set_env("BAD_FLOAT", "3.5.1");

    EXPECT_THROW((void)EnvLoader::get<int>("BAD_INT", 0), std::invalid_argument);
    EXPECT_THROW((void)EnvLoader::get<bool>("BAD_BOOL", false), std::invalid_argument);
    EXPECT_THROW((void)EnvLoader::get<double>("BAD_FLOAT", 0.0), std::invalid_argument);
}
