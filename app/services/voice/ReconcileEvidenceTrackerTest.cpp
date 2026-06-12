#include "app/services/voice/ReconcileEvidenceTracker.h"

#include <gtest/gtest.h>

#include <chrono>

namespace app::services::voice {
namespace {

using namespace std::chrono_literals;

TEST(ReconcileQuerySummaryTest, RequiresEveryConfiguredEndpointToConfirmAbsence) {
    EXPECT_FALSE((ReconcileQuerySummary{.configured_queries = 1,
                                        .successful_queries = 1,
                                        .participant_count = 1})
                     .unanimously_absent());
    EXPECT_FALSE((ReconcileQuerySummary{.configured_queries = 2,
                                        .successful_queries = 1,
                                        .failed_queries = 1})
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
    EXPECT_TRUE(
        tracker.observe_participant_missing(channel, user, "participant_missing", now + 60s)
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
    EXPECT_TRUE(
        tracker.observe_participant_missing(channel, user, "participant_missing", now + 60s)
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
    EXPECT_TRUE(
        tracker.observe_participant_missing(channel, user, "participant_missing", now + 60s)
            .first_observation);
}

}  // namespace
}  // namespace app::services::voice
