#include "livekit/routing/LivekitNodeRegistry.h"

#include <limits>
#include <stdexcept>

namespace livekit {

void LivekitNodeRegistry::register_node(const LivekitNode& node)
{
    std::lock_guard lock(mutex_);

    auto ptr = std::make_shared<LivekitNode>(node);
    if (nodes_.find(node.node_id) != nodes_.end()) {
        throw std::runtime_error("Duplicate LiveKit node id");
    }
    nodes_[node.node_id] = std::move(ptr);
}

std::shared_ptr<const LivekitNode>
LivekitNodeRegistry::get_node(const std::string& node_id) const
{
    std::lock_guard lock(mutex_);

    auto it = nodes_.find(node_id);
    if (it == nodes_.end())
        return nullptr;

    return it->second;
}

std::shared_ptr<const LivekitNode>
LivekitNodeRegistry::get_room_node(const ChannelId& room) const
{
    std::lock_guard lock(mutex_);

    auto room_it = room_to_node_.find(room);
    if (room_it == room_to_node_.end())
        return nullptr;

    auto node_it = nodes_.find(room_it->second);
    if (node_it == nodes_.end())
        return nullptr;

    return node_it->second;
}

void LivekitNodeRegistry::bind_room(const ChannelId& room,
                                    const std::string& node_id)
{
    std::lock_guard lock(mutex_);

    auto node_it = nodes_.find(node_id);
    if (node_it == nodes_.end())
        return;

    room_to_node_[room] = node_id;

    auto& node = *node_it->second;
    node.active_rooms++;
    node.update_load();
}

void LivekitNodeRegistry::clear_room(const ChannelId& room)
{
    std::lock_guard lock(mutex_);

    auto room_it = room_to_node_.find(room);
    if (room_it == room_to_node_.end())
        return;

    auto node_it = nodes_.find(room_it->second);
    if (node_it != nodes_.end()) {
        auto& node = *node_it->second;

        if (node.active_rooms > 0)
            node.active_rooms--;

        node.update_load();
    }

    room_to_node_.erase(room_it);
}

void LivekitNodeRegistry::increment_room(const std::string& node_id)
{
    std::lock_guard lock(mutex_);

    auto it = nodes_.find(node_id);
    if (it == nodes_.end())
        return;

    auto& node = *it->second;
    node.active_rooms++;
    node.update_load();
}

void LivekitNodeRegistry::decrement_room(const std::string& node_id)
{
    std::lock_guard lock(mutex_);

    auto it = nodes_.find(node_id);
    if (it == nodes_.end())
        return;

    auto& node = *it->second;

    if (node.active_rooms > 0)
        node.active_rooms--;

    node.update_load();
}

void LivekitNodeRegistry::increment_user(const std::string& node_id)
{
    std::lock_guard lock(mutex_);

    auto it = nodes_.find(node_id);
    if (it == nodes_.end())
        return;

    auto& node = *it->second;
    node.active_users++;
    node.update_load();
}

void LivekitNodeRegistry::decrement_user(const std::string& node_id)
{
    std::lock_guard lock(mutex_);

    auto it = nodes_.find(node_id);
    if (it == nodes_.end())
        return;

    auto& node = *it->second;

    if (node.active_users > 0)
        node.active_users--;

    node.update_load();
}

std::shared_ptr<const LivekitNode> LivekitNodeRegistry::pick_node() const
{
    std::lock_guard lock(mutex_);

    std::shared_ptr<LivekitNode> best;
    double best_score = std::numeric_limits<double>::max();

    for (const auto& [id, node_ptr] : nodes_) {
        if (!best || node_ptr->load_score < best_score) {
            best = node_ptr;
            best_score = node_ptr->load_score;
        }
    }

    return best;
}

std::vector<LivekitNode> LivekitNodeRegistry::list_nodes() const
{
    std::lock_guard lock(mutex_);

    std::vector<LivekitNode> result;
    result.reserve(nodes_.size());

    for (const auto& [id, node_ptr] : nodes_) {
        result.push_back(*node_ptr);
    }

    return result;
}

} // namespace livekit