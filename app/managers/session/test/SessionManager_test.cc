#include "app/managers/session/SessionManager.h"
#include "app/services/voice/VoiceSessionManager.h"

#include <gtest/gtest.h>

namespace app {
namespace {

using services::voice::VoiceSessionManager;

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

TEST(SessionManagerTest, SessionLimitAllowsReconnectAfterPreviousSessionDisconnects) {
    SessionManager manager(/*max_sessions_per_user=*/1);
    const UserId user{"user-1"};
    const auto conn1 = make_conn("conn-1");
    const auto conn2 = make_conn("conn-2");

    const auto first = manager.attachConnection(conn1, user);
    ASSERT_TRUE(first.has_value());

    manager.removeConnection(conn1);

    const auto second = manager.attachConnection(conn2, user);
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(*first, *second);
    EXPECT_TRUE(manager.hasSession(user));
}

TEST(SessionManagerTest, TransferOwnerSessionUpdatesOwnerWhenExpectedMatches) {
    VoiceSessionManager manager;
    const UserId user{"user-1"};
    const ChannelId channel{"channel-1"};
    const SessionId old_owner = 101;
    const SessionId new_owner = 202;

    manager.join(channel, user, old_owner);
    EXPECT_TRUE(manager.set_muted(user, true));
    EXPECT_TRUE(manager.transfer_owner_session(user, old_owner, new_owner));

    const auto owner = manager.user_session(user);
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(*owner, new_owner);

    const auto participants = manager.participants_in_channel(channel);
    ASSERT_EQ(participants.size(), 1U);
    EXPECT_EQ(participants.front().user_id, user);
    EXPECT_TRUE(participants.front().muted);
    EXPECT_FALSE(participants.front().deafened);
}

TEST(SessionManagerTest, TransferOwnerSessionRejectsMismatchedExpectedOwner) {
    VoiceSessionManager manager;
    const UserId user{"user-1"};
    const ChannelId channel{"channel-1"};
    const SessionId old_owner = 101;
    const SessionId wrong_expected_owner = 999;
    const SessionId new_owner = 202;

    manager.join(channel, user, old_owner);
    EXPECT_FALSE(manager.transfer_owner_session(user, wrong_expected_owner, new_owner));

    const auto owner = manager.user_session(user);
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(*owner, old_owner);
}

TEST(SessionManagerTest, TransferOwnerSessionRejectsMissingUser) {
    VoiceSessionManager manager;
    const UserId missing_user{"missing-user"};
    EXPECT_FALSE(manager.transfer_owner_session(missing_user, 100, 200));
}

TEST(SessionManagerTest, LeaveIfOwnerRequiresMatchingSession) {
    VoiceSessionManager manager;
    const UserId user{"user-1"};
    const ChannelId channel{"channel-1"};

    manager.join(channel, user, 101);

    const auto wrong_owner_result = manager.leave_if_owner(user, 202);
    EXPECT_FALSE(wrong_owner_result.removed);
    EXPECT_EQ(manager.active_voice_user_count(), 1U);

    const auto owner_result = manager.leave_if_owner(user, 101);
    EXPECT_TRUE(owner_result.removed);
    ASSERT_TRUE(owner_result.channel.has_value());
    EXPECT_EQ(*owner_result.channel, channel);
    EXPECT_TRUE(owner_result.became_empty);
    EXPECT_EQ(manager.active_voice_user_count(), 0U);
}

TEST(SessionManagerTest, JoinCarriesMuteStateWhenSameSessionMovesChannels) {
    VoiceSessionManager manager;
    const UserId user{"user-1"};
    const ChannelId old_channel{"channel-a"};
    const ChannelId new_channel{"channel-b"};
    const SessionId owner_session = 101;

    manager.join(old_channel, user, owner_session);
    EXPECT_TRUE(manager.set_muted(user, true));
    EXPECT_TRUE(manager.set_deafened(user, true));

    manager.join(new_channel, user, owner_session);

    const auto participants = manager.participants_in_channel(new_channel);
    ASSERT_EQ(participants.size(), 1U);
    EXPECT_TRUE(participants.front().muted);
    EXPECT_TRUE(participants.front().deafened);
}

TEST(SessionManagerTest, JoinDoesNotCarryMuteStateAcrossOwnerSessionChange) {
    VoiceSessionManager manager;
    const UserId user{"user-1"};
    const ChannelId old_channel{"channel-a"};
    const ChannelId new_channel{"channel-b"};

    manager.join(old_channel, user, 101);
    EXPECT_TRUE(manager.set_muted(user, true));

    manager.join(new_channel, user, 202);

    const auto participants = manager.participants_in_channel(new_channel);
    ASSERT_EQ(participants.size(), 1U);
    EXPECT_FALSE(participants.front().muted);
    EXPECT_FALSE(participants.front().deafened);
}

TEST(SessionManagerTest, ActiveVoiceUserCountTracksJoinMoveAndLeave) {
    VoiceSessionManager manager;
    const UserId user{"user-1"};
    const ChannelId channel_a{"channel-a"};
    const ChannelId channel_b{"channel-b"};
    const SessionId session_id = 101;

    EXPECT_EQ(manager.active_voice_user_count(), 0U);

    manager.join(channel_a, user, session_id);
    EXPECT_EQ(manager.active_voice_user_count(), 1U);

    manager.join(channel_b, user, session_id);
    EXPECT_EQ(manager.active_voice_user_count(), 1U);

    manager.leave(channel_b, user);
    EXPECT_EQ(manager.active_voice_user_count(), 0U);
}

TEST(SessionManagerTest, ActiveVoiceUserCountTracksMultipleUniqueUsers) {
    VoiceSessionManager manager;
    const ChannelId channel{"channel-1"};

    manager.join(channel, UserId{"user-1"}, 101);
    manager.join(channel, UserId{"user-2"}, 102);
    EXPECT_EQ(manager.active_voice_user_count(), 2U);

    manager.leave(channel, UserId{"user-1"});
    EXPECT_EQ(manager.active_voice_user_count(), 1U);
}

TEST(SessionManagerTest, ActiveVoiceUserCountUnaffectedByOwnerTransfer) {
    VoiceSessionManager manager;
    const UserId user{"user-1"};
    const ChannelId channel{"channel-1"};

    manager.join(channel, user, 101);
    EXPECT_EQ(manager.active_voice_user_count(), 1U);

    EXPECT_TRUE(manager.transfer_owner_session(user, 101, 202));
    EXPECT_EQ(manager.active_voice_user_count(), 1U);
}

}  // namespace
}  // namespace app
