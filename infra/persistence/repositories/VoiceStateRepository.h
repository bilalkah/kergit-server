#ifndef INFRA_PERSISTENCE_REPOSITORIES_VOICE_STATE_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_VOICE_STATE_REPOSITORY_H

#include "domains/ids/Ids.h"
#include "infra/persistence/DatabaseExecutor.h"

#include <vector>

class VoiceStateRepository {
   public:
    explicit VoiceStateRepository(DatabaseExecutor& db) : db_(db) {}

    struct VoiceStateRow {
        UserId user_id;
        ChannelId channel_id;
        bool muted = false;
        bool deafened = false;
    };

    /// UPSERT: insert or update user's voice state when joining a channel.
    void upsert(const UserId& user, const ChannelId& channel, bool muted, bool deafened);

    /// UPDATE muted/deafened flags for an existing row.
    void update_mute_state(const UserId& user, bool muted, bool deafened);

    /// DELETE: remove user's voice state when leaving.
    void remove(const UserId& user);

    /// DELETE all rows for a channel (when channel empties).
    void remove_channel(const ChannelId& channel);

    /// SELECT all rows (for recovery after server restart).
    std::vector<VoiceStateRow> load_all();

    /// DELETE all rows (cleanup after recovery load).
    void clear_all();

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_VOICE_STATE_REPOSITORY_H
