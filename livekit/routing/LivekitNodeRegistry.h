#ifndef LIVEKIT_ROUTING_LIVEKITNODEREGISTRY_H_
#define LIVEKIT_ROUTING_LIVEKITNODEREGISTRY_H_

#include "domains/ids/Ids.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace livekit {

struct LivekitNode {
    std::string node_id;

    // Used by clients
    std::string public_host;

    // Used by backend Twirp calls
    std::string private_host;

    size_t active_rooms = 0;
    size_t active_users = 0;

    double load_score = 0.0;

    void update_load() {
        // example weighting
        load_score = static_cast<double>(active_users) + static_cast<double>(active_rooms) * 0.25;
    }
};

class LivekitNodeRegistry {
   public:
    void register_node(const LivekitNode& node);

    std::shared_ptr<const LivekitNode> get_node(const std::string& node_id) const;

    std::shared_ptr<const LivekitNode> get_room_node(const ChannelId& room) const;

    // Bind room -> node mapping only. Room counters are managed by room lifecycle events.
    void bind_room(const ChannelId& room, const std::string& node_id);

    void clear_room(const ChannelId& room);

    void increment_room(const ChannelId& room, const std::string& node_id);
    void decrement_room(const ChannelId& room);

    void increment_user(const std::string& node_id);
    void decrement_user(const std::string& node_id);

    std::shared_ptr<const LivekitNode> pick_node() const;

    std::vector<LivekitNode> list_nodes() const;

   private:
    mutable std::mutex mutex_;

    std::unordered_map<std::string, std::shared_ptr<LivekitNode>> nodes_;
    std::vector<std::string> node_order_;
    std::unordered_map<ChannelId, std::string> room_to_node_;
    std::unordered_map<ChannelId, std::string> counted_room_to_node_;
};

}  // namespace livekit

#endif  // LIVEKIT_ROUTING_LIVEKITNODEREGISTRY_H_
