# Public ID Mapping System (nanoid)

## Context
Internal DB IDs (UUIDs) are currently sent directly to clients. We want to introduce short, opaque public IDs generated via nanoid so clients never see internal IDs. Mappings are cached in Redis with reference-counted lifecycle: each ID tracks how many active sessions reference it. When refcount hits 0, a 5-minute TTL is set. If still 0 after 5 minutes, the mapping is evicted.

## Design Decisions
- **Nanoid config**: URL-safe alphabet (`A-Za-z0-9_-`), 12 characters, unique per entity type
- **Redis + in-memory working set**: Redis stores all mappings (source of truth). In-memory map holds refcounted entries: any ID with refcount > 0 is cached locally for zero-latency lookups. Cold lookups (refcount 0) fall through to Redis. When refcount hits 0 + 5-min timeout, evict from both local map and Redis.
- **Reference-counted lifecycle**: Each ID has a refcount (number of active sessions that need it). Refcount incremented on bootstrap/join, decremented on disconnect/leave. When refcount → 0, start 5-min TTL. If someone reconnects before expiry, refcount goes back up and TTL is cleared.
- **Translation in app layer only**: Inbound: resolve public → internal at the TOP of each command's `execute()`. Outbound: translate internal → public when filling proto structures in the same command. No middleware/interceptor — all conversion happens explicitly inside commands.

## Phase 1: Foundation

### 1.1 Nanoid BUILD target
- **File**: `third_party/nanoid/BUILD` — add `cc_library` alias exposing `@nanoid//:nanoid`

### 1.2 Extend RedisClient
- **File**: `infra/redis/RedisClient.h`, `infra/redis/RedisClient.cpp`
- Add: `expire(key, ttl)`, `persist(key)`, `mget(keys)`, `set(key, value)` (no TTL variant)
- `redis-plus-plus` already supports all of these

### 1.3 Create PublicIdMapper service
- **New files**: `app/services/pubid/PublicIdMapper.h`, `.cpp`, `BUILD`
- Depends on: `@nanoid`, `//infra/redis`

```cpp
enum class EntityType : uint8_t { Hub, Channel, User, Message };

class PublicIdMapper {
public:
    PublicIdMapper(infra::redis::RedisClient& redis);

    // Core API — get-or-create public ID, resolve back to internal
    std::string to_public(EntityType type, std::string_view internal_id);
    std::optional<std::string> to_internal(EntityType type, std::string_view public_id);

    // Typed convenience
    std::string pub_hub(const HubId&);
    std::string pub_channel(const ChannelId&);
    std::string pub_user(const UserId&);
    std::string pub_message(const MessageId&);
    HubId int_hub(std::string_view pub);
    ChannelId int_channel(std::string_view pub);
    UserId int_user(std::string_view pub);
    MessageId int_message(std::string_view pub);

    // Reference counting — called on bootstrap/join and disconnect/leave
    void add_refs(const std::vector<std::pair<EntityType, std::string>>& ids);
    void remove_refs(const std::vector<std::pair<EntityType, std::string>>& ids);

private:
    infra::redis::RedisClient& redis_;

    // In-memory working set: separate maps per entity type
    struct CacheEntry {
        std::string public_id;
        int refcount = 0;
    };

    struct TypeCache {
        mutable std::shared_mutex mu;
        std::unordered_map<std::string, CacheEntry> forward;   // internal_id → {public, refcount}
        std::unordered_map<std::string, std::string> reverse;  // public_id → internal_id
    };

    TypeCache hub_cache_;
    TypeCache channel_cache_;
    TypeCache user_cache_;
    TypeCache message_cache_;

    TypeCache& cache_for(EntityType type);  // returns the appropriate TypeCache
};
```

- **Redis key scheme**: `pub:h:{internal}` → public, `rev:h:{public}` → internal (h=hub, c=channel, u=user, m=message)
- **Fast path (99%+ of lookups)**: Check type-specific `forward` map → return immediately, zero Redis calls
- **Cold miss (server restart / first encounter)**: Local miss → `redis_.get(key)`. If found, populate local entry. If not, generate nanoid, write to Redis + local.
- **Batch cold load**: On bootstrap, use `redis_.mget(keys)` to fetch all mappings for a hub's entities in one Redis round-trip, then populate local maps.
- **Redis role**: Persistence only — ensures mappings survive server restarts. All steady-state lookups are local.
- **Thread safety**: Per-type `shared_mutex` — `shared_lock` for reads (hot path), `unique_lock` for writes/refcount changes. Different entity types don't contend.

### Reference counting logic
- `add_refs(ids)`: For each ID, increment refcount in local map. If was 0 → call `redis_.persist(fwd_key)` and `redis_.persist(rev_key)` to clear any pending TTL. If not in local map, fetch from Redis first.
- `remove_refs(ids)`: Decrement refcount. If hits 0 → call `redis_.expire(fwd_key, 300s)` and `redis_.expire(rev_key, 300s)`. Schedule local eviction after 5 min (check refcount still 0 before evicting).
- Refcounts are ephemeral — on server restart they reset to 0. Bootstrap re-establishes them for all online users.

## Phase 2: Wire into CommandContext

### 2.1 Add to CommandContext
- **File**: `app/dispatcher/CommandContext.h`
- Add `services::pubid::PublicIdMapper& public_id_mapper;`

### 2.2 Instantiate in AppStack
- **File**: `app/AppStack.h`, `app/AppStack.cpp`
- Add `std::unique_ptr<services::pubid::PublicIdMapper> public_id_mapper_;`
- Create in `init_services()`, pass `*redis_client_`
- Wire into `cmd_ctx_`

## Phase 3: Inbound Translation (public → internal) — at top of commands

Each command that receives IDs from the client resolves them at the start of `execute()`:

```cpp
void SomeCommand::execute(const ParsedPayload& payload, CommandContext& ctx, ...) {
    auto& cmd = std::get<SomeProto>(payload);
    auto hub_id = ctx.public_id_mapper.int_hub(cmd.hub_id());
    auto channel_id = ctx.public_id_mapper.int_channel(cmd.channel_id());
    // ... use internal IDs for all business logic
}
```

Commands that receive public IDs from clients:
- Activity: `JoinVoiceChannelCommand`, `VoiceChannelActivityCommand`, `TypingCommand`, `SelectActiveChannelCommand`
- Channel: `CreateChannelCommand`, `RemoveChannelCommand`, `RenameChannelCommand`
- Hub: `CreateHubCommand` (no inbound IDs), `UpdateHubCommand`, `DeleteHubCommand`, `JoinHubByInviteCommand`, `LeaveHubCommand`, `GetHubInviteCommand`
- Message: `SendMessageCommand`, `FetchLatestMessagesCommand`, `FetchMessagesBeforeCommand`
- User: `UpdateUserCommand`
- Member: `UpdateMemberRoleCommand`

## Phase 4: Outbound Translation (internal → public) — when filling protos

In the same commands, when building response protos, translate internal → public:

### 4.1 Shared helpers
- **File**: `app/commands/utils.h`
  - `to_proto_message()` — add `PublicIdMapper&` param, translate message.id, sender_id, author.id
  - `make_voice_activity()` — add `PublicIdMapper&` param, translate hub_id, channel_id, user_id
  - All other helper functions that set IDs on protos

### 4.2 HubNotifier
- **File**: `app/services/hub/HubNotifier.h`, `app/services/hub/HubNotifier.cpp`
- Add `PublicIdMapper&` constructor param
- Translate IDs in: `hubUpdated`, `hubRemoved`, `memberJoined`, `memberLeft`, `memberOnline`, `memberOffline`, `channelCreated`, `channelUpdated`, `channelRemoved`

### 4.3 Command handlers (outbound proto filling)
- `app/commands/session/BootstrapCommand.cpp` — self user, all hubs/channels/members/users/voice
- `app/commands/session/DisconnectionCommand.cpp` — voice participant updates
- `app/commands/message/SendMessageCommand.cpp`
- `app/commands/message/FetchLatestMessagesCommand.cpp`
- `app/commands/message/FetchMessagesBeforeCommand.cpp`
- `app/commands/hub/CreateHubCommand.cpp`
- `app/commands/hub/UpdateHubCommand.cpp`
- `app/commands/hub/DeleteHubCommand.cpp`
- `app/commands/hub/JoinHubByInviteCommand.cpp`
- `app/commands/hub/LeaveHubCommand.cpp`
- `app/commands/hub/GetHubInviteCommand.cpp`
- `app/commands/channel/RemoveChannelCommand.cpp`
- `app/commands/channel/RenameChannelCommand.cpp`
- `app/commands/activity/JoinVoiceChannelCommand.cpp`
- `app/commands/activity/VoiceChannelActivityCommand.cpp`
- `app/commands/activity/TypingCommand.cpp`
- `app/commands/activity/SelectActiveChannelCommand.cpp`
- `app/commands/user/UpdateUserCommand.cpp`
- `app/commands/member/UpdateMemberRoleCommand.cpp`
- `app/commands/session/AuthenticateCommand.cpp`

## Phase 5: Cache Lifecycle Integration

### 5.1 Bootstrap — add refs
In `BootstrapCommand`, after processing each hub, collect all IDs the user needs and call `add_refs()`:
- The hub ID itself
- All channel IDs in the hub
- All member user IDs in the hub
- The user's own ID

### 5.2 Disconnect — remove refs
In `DisconnectionCommand`, when user goes fully offline (`!hasSession`):
- For each hub the user was subscribed to: collect same set of IDs and call `remove_refs()`
- This decrements refcounts. If any hit 0, the 5-min TTL kicks in.

### 5.3 Join/Leave hub — adjust refs
- `JoinHubByInviteCommand`: `add_refs()` for the new hub's IDs + the joining user's ID for all existing members
- `LeaveHubCommand`: `remove_refs()` for the hub's IDs from the leaving user's perspective

### 5.4 New entity creation
- `CreateHubCommand`, `CreateChannelCommand`, `SendMessageCommand`: generate public ID via `to_public()`, refcount starts at 0 but gets incremented by the broadcast recipients' sessions

## Verification
1. `bazel build --config=vanilla //app/services/pubid:pubid` — new library compiles
2. `bazel build --config=vanilla //server:fake_discord` — full server builds
3. `bazel test --config=testing //...` — all existing tests pass
4. Manual test: connect web client, inspect WebSocket frames — IDs should be 12-char nanoids, not UUIDs
5. Verify round-trip: client sends command with public ID → server resolves to internal → processes correctly
6. Verify cache: disconnect all users from a hub → wait 5 min → check Redis keys are gone
