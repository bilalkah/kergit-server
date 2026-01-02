#ifndef APP_DISPATCHER_COMMANDCONTEXT_H
#define APP_DISPATCHER_COMMANDCONTEXT_H

// include services headers
#include "app/services/PublicIdService.h"
#include "app/services/auth/AuthService.h"
#include "app/services/channel/ChannelService.h"
#include "app/services/hub/HubNotifier.h"
#include "app/services/hub/HubService.h"
#include "app/services/hub/SnapshotBuilder.h"
#include "app/services/presence/PresenceService.h"
#include "app/services/user/UserService.h"

// include managers headers
#include "app/managers/session/SessionManager.h"
#include "app/managers/subscription/SubscriptionManager.h"


namespace app {

struct CommandContext {
    services::PublicIdService& ids;
    services::AuthService& auth_service;
    services::ChannelService& channel_service;
    services::HubService& hub_service;
    services::HubNotifier& hub_notifier;
    services::HubSnapshotBuilder& hub_snapshot_builder;
    services::UserService& user_service;
    services::PresenceService& presence_manager;

    SubscriptionManager& subscription_manager;
    SessionManager& session_manager;
};

}  // namespace app

#endif  // APP_DISPATCHER_COMMANDCONTEXT_H
