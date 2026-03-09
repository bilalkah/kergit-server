#ifndef APP_DISPATCHER_COMMANDCONTEXT_H
#define APP_DISPATCHER_COMMANDCONTEXT_H

// include queue headers
#include "app/queue/IEventSink.h"

// include services headers
#include "app/services/auth/AuthService.h"
#include "app/services/invite/InviteService.h"
#include "app/services/channel/ChannelService.h"
#include "app/services/hub/HubNotifier.h"
#include "app/services/hub/HubService.h"
#include "app/services/hub/SnapshotBuilder.h"
#include "app/services/voice/VoiceService.h"
#include "app/services/presence/PresenceService.h"
#include "app/services/user/UserService.h"

// include managers headers
#include "app/managers/session/SessionManager.h"
#include "app/managers/subscription/SubscriptionManager.h"

namespace app {

struct CommandContext {
    services::AuthService& auth_service;
    services::ChannelService& channel_service;
    services::HubService& hub_service;
    services::HubNotifier& hub_notifier;
    services::HubSnapshotBuilder& hub_snapshot_builder;
    services::voice::VoiceService& voice_service;
    services::UserService& user_service;
    services::PresenceService& presence_manager;

    SubscriptionManager& subscription_manager;
    SessionManager& session_manager;

    // Event sink for pushing follow-up commands
    queue::IEventSink& event_sink;

    services::InviteService& invite_service;
};

}  // namespace app

#endif  // APP_DISPATCHER_COMMANDCONTEXT_H
