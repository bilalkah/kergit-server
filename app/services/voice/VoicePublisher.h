#ifndef APP_SERVICES_VOICE_VOICEPUBLISHER_H_
#define APP_SERVICES_VOICE_VOICEPUBLISHER_H_

#include "app/managers/session/SessionManager.h"
#include "app/managers/subscription/SubscriptionManager.h"
#include "app/services/hub/HubService.h"
#include "app/services/voice/VoiceResumeRegistry.h"
#include "app/services/voice/VoiceSessionManager.h"
#include "domains/ids/Ids.h"
#include "net/outbound/IOutBoundSink.h"

#include <cstdint>
#include <optional>
#include <string>

namespace app::services::voice {

/**
 * VoicePublisher
 *
 * The single place that turns voice state changes into protobuf envelopes and fans them
 * out via the outbound sink. Hub-wide deltas go to channel subscribers; key updates and
 * self-status go to the relevant participant sessions.
 */
class VoicePublisher {
   public:
    VoicePublisher(net::outbound::IOutboundSink& outbound_sink,
                   SubscriptionManager& subscription_manager, SessionManager& session_manager,
                   app::services::HubService& hub_service, VoiceSessionManager& sessions,
                   VoiceResumeRegistry& resume_registry);

    void publish_voice_snapshot(const HubId& hub, const ChannelId& channel,
                                uint64_t started_at_unix);
    void publish_voice_participant_upsert(const HubId& hub, const ChannelId& channel,
                                          const UserId& user, bool muted, bool deafened);
    void publish_voice_participant_remove(const HubId& hub, const ChannelId& channel,
                                          const UserId& user);

    // Broadcast the channel key+index to every current participant's owner session.
    void publish_voice_key_update(const HubId& hub, const ChannelId& channel,
                                  const std::string& key, uint32_t key_index);
    // Send the channel key+index to a single user's owner session (resume re-sync).
    void publish_voice_key_update_to_user(const HubId& hub, const ChannelId& channel,
                                          const UserId& user, const std::string& key,
                                          uint32_t key_index);

    void publish_self_status(const UserId& user, bool connected,
                             const std::optional<SessionId>& owner_session_id,
                             const std::optional<ChannelId>& channel,
                             std::optional<SessionId> only_session_id = std::nullopt);

   private:
    void emit(net::outbound::OutgoingMessage msg);

    net::outbound::IOutboundSink& outbound_sink_;
    SubscriptionManager& subscription_manager_;
    SessionManager& session_manager_;
    app::services::HubService& hub_service_;
    VoiceSessionManager& sessions_;
    VoiceResumeRegistry& resume_registry_;
};

}  // namespace app::services::voice

#endif  // APP_SERVICES_VOICE_VOICEPUBLISHER_H_
