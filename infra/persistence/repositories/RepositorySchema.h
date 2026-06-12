#ifndef INFRA_PERSISTENCE_REPOSITORY_SCHEMA_H
#define INFRA_PERSISTENCE_REPOSITORY_SCHEMA_H

namespace dbschema {

// Application schema.
//
// New durable application data lives under kergit_app.
// Legacy public.* tables must not be used by newly migrated repositories.
inline constexpr const char* kAppSchema = "kergit_app";

// External schemas kept intentionally separate.
inline constexpr const char* kAuthUsers = "auth.users";
inline constexpr const char* kStorageBuckets = "storage.buckets";

// kergit_app tables.
inline constexpr const char* kProfiles = "kergit_app.profiles";

inline constexpr const char* kHubRoles = "kergit_app.hub_roles";
inline constexpr const char* kHubs = "kergit_app.hubs";
inline constexpr const char* kHubMembers = "kergit_app.hub_members";

inline constexpr const char* kChannels = "kergit_app.channels";

inline constexpr const char* kChannelMessageCounters = "kergit_app.channel_message_counters";
inline constexpr const char* kMessages = "kergit_app.messages";
inline constexpr const char* kMessageAttachments = "kergit_app.message_attachments";

inline constexpr const char* kAuditEvents = "kergit_app.audit_events";

inline constexpr const char* kAccountDeletionRequests = "kergit_app.account_deletion_requests";
inline constexpr const char* kAccountDeletedEmailReservations =
    "kergit_app.account_deleted_email_reservations";

inline constexpr const char* kLegalTermsAcceptances = "kergit_app.legal_terms_acceptances";
inline constexpr const char* kPrivacyNoticeDeliveries = "kergit_app.privacy_notice_deliveries";

// kergit_app functions.
inline constexpr const char* kGetHubRoleId = "kergit_app.get_hub_role_id";
inline constexpr const char* kPurgeOldAuditEvents = "kergit_app.purge_old_audit_events";

}  // namespace dbschema

#endif  // INFRA_PERSISTENCE_REPOSITORY_SCHEMA_H
