#include "app/managers/session/SessionManager.h"

#include <gtest/gtest.h>

namespace app {
namespace {

GlobalConnId make_conn(const char* id) {
    return GlobalConnId{NetStackId{"stack-1"}, ConnId{id}};
}

SessionId expect_attach_ok(SessionManager& manager, const GlobalConnId& conn, const UserId& user) {
    auto result = manager.attachConnection(conn, user);
    EXPECT_TRUE(result.has_value());
    return result.value_or(0);
}

TEST(SessionManagerTest, AttachConnectionMintsUniqueSessionIdsForDifferentConnections) {
    SessionManager manager;
    const UserId user{"user-1"};
    const auto conn1 = make_conn("conn-1");
    const auto conn2 = make_conn("conn-2");

    const SessionId session1 = expect_attach_ok(manager, conn1, user);
    const SessionId session2 = expect_attach_ok(manager, conn2, user);

    EXPECT_NE(session1, session2);
    EXPECT_LT(session1, session2);

    auto stored1 = manager.sessionIdOfConnection(conn1);
    auto stored2 = manager.sessionIdOfConnection(conn2);
    ASSERT_TRUE(stored1.has_value());
    ASSERT_TRUE(stored2.has_value());
    EXPECT_EQ(*stored1, session1);
    EXPECT_EQ(*stored2, session2);
}

TEST(SessionManagerTest, AttachConnectionIsIdempotentForSameConnection) {
    SessionManager manager;
    const UserId user{"user-1"};
    const auto conn = make_conn("conn-1");

    const SessionId session1 = expect_attach_ok(manager, conn, user);
    const SessionId session2 = expect_attach_ok(manager, conn, user);

    EXPECT_EQ(session1, session2);
    EXPECT_TRUE(manager.hasSession(user));
    EXPECT_EQ(manager.activeUsers().size(), 1U);
}

TEST(SessionManagerTest, RemovingOneConnectionKeepsOtherSessionsAlive) {
    SessionManager manager;
    const UserId user{"user-1"};
    const auto conn1 = make_conn("conn-1");
    const auto conn2 = make_conn("conn-2");

    const SessionId session1 = expect_attach_ok(manager, conn1, user);
    const SessionId session2 = expect_attach_ok(manager, conn2, user);
    manager.removeConnection(conn1);

    EXPECT_FALSE(manager.sessionOfConnection(conn1).has_value());
    EXPECT_FALSE(manager.sessionIdOfConnection(conn1).has_value());
    EXPECT_TRUE(manager.sessionOfConnection(conn2).has_value());
    EXPECT_TRUE(manager.hasSession(user));
    EXPECT_EQ(manager.activeUsers().size(), 1U);
    EXPECT_FALSE(manager.sessionIdHasConnections(session1));
    EXPECT_TRUE(manager.sessionIdHasConnections(session2));

    const auto user_sessions = manager.getUserSessionIds(user);
    ASSERT_EQ(user_sessions.size(), 1U);
    EXPECT_EQ(user_sessions.front(), session2);
}

TEST(SessionManagerTest, RemovesActiveSessionWhenLastConnectionDisconnects) {
    SessionManager manager;
    const UserId user{"user-1"};
    const auto conn1 = make_conn("conn-1");
    const auto conn2 = make_conn("conn-2");

    expect_attach_ok(manager, conn1, user);
    expect_attach_ok(manager, conn2, user);

    manager.removeConnection(conn1);
    manager.removeConnection(conn2);

    EXPECT_FALSE(manager.hasSession(user));
    EXPECT_TRUE(manager.getUserSessionIds(user).empty());
    EXPECT_TRUE(manager.activeUsers().empty());
    EXPECT_FALSE(manager.getSession(user).has_value());
}

TEST(SessionManagerTest, AttachConnectionRejectsWhenSessionLimitExceeded) {
    SessionManager manager(/*max_sessions_per_user=*/1);
    const UserId user{"user-1"};

    const auto first = manager.attachConnection(make_conn("conn-1"), user);
    ASSERT_TRUE(first.has_value());

    const auto second = manager.attachConnection(make_conn("conn-2"), user);
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), sercom::protocol::event::CommandErrorCode_SESSION_LIMIT_EXCEEDED);
}

}  // namespace
}  // namespace app
