#include <exception>
#include <gtest/gtest.h>
#include <pqxx/connection>

// A simple test to check if pqxx::connection can be constructed.
TEST(LibpqxxConnectionTest, CanConstructConnection) {
    try {
        pqxx::connection c{"host=127.0.0.1 port=5432 user=bilal dbname=postgres"};
        SUCCEED() << "libpqxx connection object created successfully";
    } catch (const std::exception& e) {
        FAIL() << "Exception during pqxx::connection construction: " << e.what();
    }
}
