#ifndef APP_DISPATCHER_COMMANDCONTEXT_H
#define APP_DISPATCHER_COMMANDCONTEXT_H

namespace app {
namespace services {
struct AuthService;
struct PresenceService;
struct ChannelService;
struct HubService;
struct UserService;
struct HubNotifier;
struct HubSnapshotBuilder;
};  // namespace services

struct SubscriptionManager;
struct SessionManager;

struct CommandContext {
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
