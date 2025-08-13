#include <gtest/gtest.h>
#include <pqxx/pqxx>
#include <string>
#include <vector>

// Assuming ChatDB and Message structs from previous example are in chatdb.h
#include "core/database/src/chatdb.h"

class ChatDBTest : public ::testing::Test {
   protected:
    // Use a test database or schema to avoid messing with production data
    ChatDB* db;

    virtual void SetUp() override {
        // Connect to your test db here, adjust connection string accordingly
        db = new ChatDB(
            "dbname=chat_db user=chat_user host=localhost port=5432");

        // Clean tables before each test to start fresh
        pqxx::work txn(db->getConnection());
        txn.exec("TRUNCATE messages, channels, hub_members, hubs, users RESTART IDENTITY CASCADE");
        txn.commit();

        // Insert test users
        pqxx::work txn2(db->getConnection());
        txn2.exec("INSERT INTO users (username, password_hash) VALUES ('owner', 'hashedpw1')");
        txn2.exec("INSERT INTO users (username, password_hash) VALUES ('member1', 'hashedpw2')");
        txn2.exec("INSERT INTO users (username, password_hash) VALUES ('member2', 'hashedpw3')");
        txn2.commit();
    }

    virtual void TearDown() override { delete db; }
};

TEST_F(ChatDBTest, CreateHubAndAddMembers) {
    int ownerId = 1;  // First user inserted
    int member1Id = 2;
    int member2Id = 3;

    int hubId = db->createHub("TestHub", ownerId);
    ASSERT_GT(hubId, 0);

    // Add members
    db->addMember(hubId, member1Id);
    db->addMember(hubId, member2Id);

    // Check if members exist in hub_members table
    pqxx::work txn(db->getConnection());
    auto result = txn.exec_params(
        "SELECT user_id, role FROM hub_members WHERE hub_id = $1 ORDER BY user_id", hubId);
    ASSERT_EQ(result.size(), 3);  // owner + 2 members

    EXPECT_EQ(result[0][0].as<int>(), ownerId);
    EXPECT_EQ(result[0][1].as<std::string>(), "owner");
    EXPECT_EQ(result[1][0].as<int>(), member1Id);
    EXPECT_EQ(result[1][1].as<std::string>(), "member");
    EXPECT_EQ(result[2][0].as<int>(), member2Id);
    EXPECT_EQ(result[2][1].as<std::string>(), "member");
}

TEST_F(ChatDBTest, CreateChannelAndSendMessage) {
    int ownerId = 1;
    int hubId = db->createHub("TestHub", ownerId);
    int channelId = db->createChannel(hubId, "general", "text");
    ASSERT_GT(channelId, 0);

    std::string testMessage = "Hello, gtest!";
    db->sendMessage(channelId, ownerId, testMessage);

    auto messages = db->fetchMessages(channelId, 10);
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].content, testMessage);
    EXPECT_EQ(messages[0].sender_id, ownerId);
    EXPECT_EQ(messages[0].channel_id, channelId);
}

TEST_F(ChatDBTest, RemoveMember) {
    int ownerId = 1;
    int memberId = 2;

    int hubId = db->createHub("TestHub", ownerId);
    db->addMember(hubId, memberId);

    // Confirm member added
    {
        pqxx::work txn(db->getConnection());
        auto result = txn.exec_params(
            "SELECT COUNT(*) FROM hub_members WHERE hub_id = $1 AND user_id = $2", hubId, memberId);
        EXPECT_EQ(result[0][0].as<int>(), 1);
    }

    db->removeMember(hubId, memberId);

    // Confirm member removed
    {
        pqxx::work txn(db->getConnection());
        auto result = txn.exec_params(
            "SELECT COUNT(*) FROM hub_members WHERE hub_id = $1 AND user_id = $2", hubId, memberId);
        EXPECT_EQ(result[0][0].as<int>(), 0);
    }
}
