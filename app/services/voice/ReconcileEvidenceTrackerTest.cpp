#include "app/services/voice/ReconcileEvidenceTracker.h"

#include "app/services/voice/VoiceSessionManager.h"

#include <chrono>
#include <gtest/gtest.h>

namespace app::services::voice {
namespace {

using namespace std::chrono_literals;

TEST(ReconcileQuerySummaryTest, RequiresEveryConfiguredEndpointToConfirmAbsence) {
    EXPECT_FALSE((ReconcileQuerySummary{
                      .configured_queries = 1, .successful_queries = 1, .participant_count = 1})
                     .unanimously_absent());
    EXPECT_FALSE((ReconcileQuerySummary{
                      .configured_queries = 2, .successful_queries = 1, .failed_queries = 1})
                     .unanimously_absent());
    EXPECT_TRUE((ReconcileQuerySummary{.configured_queries = 2, .successful_queries = 2})
                    .unanimously_absent());
}

TEST(ReconcileEvidenceTrackerTest, OneEmptyResultDoesNotConfirmMissing) {
    ReconcileEvidenceTracker tracker(60s);
    const auto now = ReconcileEvidenceTracker::Clock::now();

    const auto result = tracker.observe_channel_missing(ChannelId{"channel"}, "periodic", now);

    EXPECT_TRUE(result.first_observation);
    EXPECT_FALSE(result.confirmed);
    ASSERT_EQ(tracker.tracked_missing_channels().size(), 1);
    EXPECT_EQ(tracker.tracked_missing_channels().front().value, "channel");
}

TEST(ReconcileEvidenceTrackerTest, AbsenceUnderGraceDoesNotConfirmMissing) {
    ReconcileEvidenceTracker tracker(60s);
    const ChannelId channel{"channel"};
    const auto now = ReconcileEvidenceTracker::Clock::now();

    tracker.observe_channel_missing(channel, "periodic", now);
    const auto result = tracker.observe_channel_missing(channel, "periodic", now + 59s);

    EXPECT_FALSE(result.first_observation);
    EXPECT_FALSE(result.confirmed);
}

TEST(ReconcileEvidenceTrackerTest, ContinuousAbsenceAtGraceConfirmsMissing) {
    ReconcileEvidenceTracker tracker(60s);
    const ChannelId channel{"channel"};
    const UserId user{"user"};
    const auto now = ReconcileEvidenceTracker::Clock::now();

    tracker.observe_channel_missing(channel, "periodic", now);
    tracker.observe_participant_missing(channel, user, "participant_missing", now);

    EXPECT_TRUE(tracker.observe_channel_missing(channel, "periodic", now + 60s).confirmed);
    EXPECT_TRUE(tracker.observe_participant_missing(channel, user, "participant_missing", now + 60s)
                    .first_observation);
}

TEST(ReconcileEvidenceTrackerTest, InconclusiveQueryResetsAbsenceTimers) {
    ReconcileEvidenceTracker tracker(60s);
    const ChannelId channel{"channel"};
    const UserId user{"user"};
    const auto now = ReconcileEvidenceTracker::Clock::now();

    tracker.observe_channel_missing(channel, "periodic", now);
    tracker.observe_participant_missing(channel, user, "participant_missing", now);
    tracker.reset_channel(channel);

    EXPECT_TRUE(tracker.observe_channel_missing(channel, "periodic", now + 60s).first_observation);
    EXPECT_TRUE(tracker.observe_participant_missing(channel, user, "participant_missing", now + 60s)
                    .first_observation);
}

TEST(ReconcileEvidenceTrackerTest, PositiveEvidenceResetsRelevantTimers) {
    ReconcileEvidenceTracker tracker(60s);
    const ChannelId channel{"channel"};
    const UserId user{"user"};
    const auto now = ReconcileEvidenceTracker::Clock::now();

    tracker.observe_channel_missing(channel, "periodic", now);
    tracker.observe_participant_missing(channel, user, "participant_missing", now);
    tracker.clear_channel_missing(channel);
    tracker.clear_participant_missing(channel, user);

    EXPECT_TRUE(tracker.tracked_missing_channels().empty());
    EXPECT_TRUE(tracker.observe_channel_missing(channel, "periodic", now + 60s).first_observation);
    EXPECT_TRUE(tracker.observe_participant_missing(channel, user, "participant_missing", now + 60s)
                    .first_observation);
}

// ---------------------------------------------------------------------------
// Reconcile cleanup policy: a single LiveKit empty/missing observation must
// never drop a user. Missing remote evidence is accumulated over the TTL, and
// only continuous confirmed absence cleans local voice state. These tests pair
// the evidence tracker with VoiceSessionManager to simulate the production
// policy without LiveKit, Redis, outbound sinks, or sleeps.
// ---------------------------------------------------------------------------

TEST(VoiceReconcileCleanupTest, ParticipantMissingFirstObservationDoesNotDropLocalUser) {
    ReconcileEvidenceTracker tracker(60s);
    VoiceSessionManager sessions;

    const ChannelId channel{"voice-1"};
    const UserId user{"user-1"};
    const SessionId session = 123;
    const auto now = ReconcileEvidenceTracker::Clock::now();

    sessions.join(channel, user, session);

    const auto observed =
        tracker.observe_participant_missing(channel, user, "participant_missing_in_livekit", now);

    if (observed.confirmed) {
        sessions.leave(channel, user);
    }

    EXPECT_FALSE(observed.confirmed);
    EXPECT_EQ(sessions.user_channel(user), channel);
    EXPECT_FALSE(sessions.is_empty(channel));
}

TEST(VoiceReconcileCleanupTest, ParticipantMissingBeforeTtlDoesNotDropLocalUser) {
    ReconcileEvidenceTracker tracker(60s);
    VoiceSessionManager sessions;

    const ChannelId channel{"voice-1"};
    const UserId user{"user-1"};
    const SessionId session = 123;
    const auto now = ReconcileEvidenceTracker::Clock::now();

    sessions.join(channel, user, session);

    tracker.observe_participant_missing(channel, user, "participant_missing_in_livekit", now);
    const auto observed = tracker.observe_participant_missing(
        channel, user, "participant_missing_in_livekit", now + 59s);

    if (observed.confirmed) {
        sessions.leave(channel, user);
    }

    EXPECT_FALSE(observed.confirmed);
    EXPECT_EQ(sessions.user_channel(user), channel);
    EXPECT_FALSE(sessions.is_empty(channel));
}

TEST(VoiceReconcileCleanupTest, ParticipantMissingAfterTtlDropsLocalUser) {
    ReconcileEvidenceTracker tracker(60s);
    VoiceSessionManager sessions;

    const ChannelId channel{"voice-1"};
    const UserId user{"user-1"};
    const SessionId session = 123;
    const auto now = ReconcileEvidenceTracker::Clock::now();

    sessions.join(channel, user, session);

    tracker.observe_participant_missing(channel, user, "participant_missing_in_livekit", now);
    const auto observed = tracker.observe_participant_missing(
        channel, user, "participant_missing_in_livekit", now + 60s);

    if (observed.confirmed) {
        sessions.leave(channel, user);
    }

    EXPECT_TRUE(observed.confirmed);
    EXPECT_FALSE(sessions.user_channel(user).has_value());
    EXPECT_TRUE(sessions.is_empty(channel));
}

TEST(VoiceReconcileCleanupTest, RoomMissingFirstObservationDoesNotClearChannel) {
    ReconcileEvidenceTracker tracker(60s);
    VoiceSessionManager sessions;

    const ChannelId channel{"voice-1"};
    const UserId user{"user-1"};
    const SessionId session = 123;
    const auto now = ReconcileEvidenceTracker::Clock::now();

    sessions.join(channel, user, session);

    const auto observed =
        tracker.observe_channel_missing(channel, "room_missing_in_livekit", now);

    if (observed.confirmed) {
        sessions.clear_channel(channel);
    }

    EXPECT_FALSE(observed.confirmed);
    EXPECT_EQ(sessions.user_channel(user), channel);
    EXPECT_FALSE(sessions.is_empty(channel));
}

TEST(VoiceReconcileCleanupTest, RoomMissingBeforeTtlDoesNotClearChannel) {
    ReconcileEvidenceTracker tracker(60s);
    VoiceSessionManager sessions;

    const ChannelId channel{"voice-1"};
    const UserId user{"user-1"};
    const SessionId session = 123;
    const auto now = ReconcileEvidenceTracker::Clock::now();

    sessions.join(channel, user, session);

    tracker.observe_channel_missing(channel, "room_missing_in_livekit", now);
    const auto observed =
        tracker.observe_channel_missing(channel, "room_missing_in_livekit", now + 59s);

    if (observed.confirmed) {
        sessions.clear_channel(channel);
    }

    EXPECT_FALSE(observed.confirmed);
    EXPECT_EQ(sessions.user_channel(user), channel);
    EXPECT_FALSE(sessions.is_empty(channel));
}

TEST(VoiceReconcileCleanupTest, RoomMissingAfterTtlClearsChannel) {
    ReconcileEvidenceTracker tracker(60s);
    VoiceSessionManager sessions;

    const ChannelId channel{"voice-1"};
    const UserId user{"user-1"};
    const SessionId session = 123;
    const auto now = ReconcileEvidenceTracker::Clock::now();

    sessions.join(channel, user, session);

    tracker.observe_channel_missing(channel, "room_missing_in_livekit", now);
    const auto observed =
        tracker.observe_channel_missing(channel, "room_missing_in_livekit", now + 60s);

    if (observed.confirmed) {
        sessions.clear_channel(channel);
    }

    EXPECT_TRUE(observed.confirmed);
    EXPECT_FALSE(sessions.user_channel(user).has_value());
    EXPECT_TRUE(sessions.is_empty(channel));
}

TEST(VoiceReconcileCleanupTest, PositiveEvidenceResetsParticipantMissingTimer) {
    ReconcileEvidenceTracker tracker(60s);
    VoiceSessionManager sessions;

    const ChannelId channel{"voice-1"};
    const UserId user{"user-1"};
    const SessionId session = 123;
    const auto now = ReconcileEvidenceTracker::Clock::now();

    sessions.join(channel, user, session);

    tracker.observe_participant_missing(channel, user, "participant_missing_in_livekit", now);
    tracker.clear_participant_missing(channel, user);
    const auto observed = tracker.observe_participant_missing(
        channel, user, "participant_missing_in_livekit", now + 60s);

    if (observed.confirmed) {
        sessions.leave(channel, user);
    }

    EXPECT_TRUE(observed.first_observation);
    EXPECT_FALSE(observed.confirmed);
    EXPECT_EQ(sessions.user_channel(user), channel);
    EXPECT_FALSE(sessions.is_empty(channel));
}

TEST(VoiceReconcileCleanupTest, PositiveEvidenceResetsRoomMissingTimer) {
    ReconcileEvidenceTracker tracker(60s);
    VoiceSessionManager sessions;

    const ChannelId channel{"voice-1"};
    const UserId user{"user-1"};
    const SessionId session = 123;
    const auto now = ReconcileEvidenceTracker::Clock::now();

    sessions.join(channel, user, session);

    tracker.observe_channel_missing(channel, "room_missing_in_livekit", now);
    tracker.clear_channel_missing(channel);
    const auto observed =
        tracker.observe_channel_missing(channel, "room_missing_in_livekit", now + 60s);

    if (observed.confirmed) {
        sessions.clear_channel(channel);
    }

    EXPECT_TRUE(observed.first_observation);
    EXPECT_FALSE(observed.confirmed);
    EXPECT_EQ(sessions.user_channel(user), channel);
    EXPECT_FALSE(sessions.is_empty(channel));
}

}  // namespace
}  // namespace app::services::voice
