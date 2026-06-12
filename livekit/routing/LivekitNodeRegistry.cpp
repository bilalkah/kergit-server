#include "livekit/routing/LivekitNodeRegistry.h"

#include <limits>
#include <stdexcept>

namespace livekit {

void LivekitNodeRegistry::register_node(const LivekitNode& node) {
    std::lock_guard lock(mutex_);

    auto ptr = std::make_shared<LivekitNode>(node);
    if (nodes_.find(node.node_id) != nodes_.end()) {
        throw std::runtime_error("Duplicate LiveKit node id");
    }
    node_order_.push_back(node.node_id);
    nodes_[node.node_id] = std::move(ptr);
}

std::shared_ptr<const LivekitNode> LivekitNodeRegistry::get_node(const std::string& node_id) const {
    std::lock_guard lock(mutex_);

    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) return nullptr;

    return it->second;
}

std::shared_ptr<const LivekitNode> LivekitNodeRegistry::get_room_node(const ChannelId& room) const {
    std::lock_guard lock(mutex_);

    auto room_it = room_to_node_.find(room);
    if (room_it == room_to_node_.end()) return nullptr;

    auto node_it = nodes_.find(room_it->second);
    if (node_it == nodes_.end()) return nullptr;

    return node_it->second;
}

void LivekitNodeRegistry::bind_room(const ChannelId& room, const std::string& node_id) {
    std::lock_guard lock(mutex_);

    auto node_it = nodes_.find(node_id);
    if (node_it == nodes_.end()) return;

    auto room_it = room_to_node_.find(room);
    if (room_it != room_to_node_.end()) {
        if (room_it->second == node_id) {
            return;  // idempotent bind
        }
    }

    auto counted_it = counted_room_to_node_.find(room);
    if (counted_it != counted_room_to_node_.end() && counted_it->second != node_id) {
        auto old_counted_it = nodes_.find(counted_it->second);
        if (old_counted_it != nodes_.end()) {
            auto& old_node = *old_counted_it->second;
            if (old_node.active_rooms > 0) {
                old_node.active_rooms--;
            }
            old_node.update_load();
        }

        auto& new_node = *node_it->second;
        new_node.active_rooms++;
        new_node.update_load();
        counted_it->second = node_id;
    }

    room_to_node_[room] = node_id;
}

void LivekitNodeRegistry::clear_room(const ChannelId& room) {
    std::lock_guard lock(mutex_);

    if (auto counted_it = counted_room_to_node_.find(room); counted_it != counted_room_to_node_.end()) {
        auto node_it = nodes_.find(counted_it->second);
        if (node_it != nodes_.end()) {
            auto& node = *node_it->second;
            if (node.active_rooms > 0) {
                node.active_rooms--;
            }
            node.update_load();
        }
        counted_room_to_node_.erase(counted_it);
    }

    room_to_node_.erase(room);
}

void LivekitNodeRegistry::increment_room(const ChannelId& room, const std::string& node_id) {
    std::lock_guard lock(mutex_);

    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) return;

    if (auto counted_it = counted_room_to_node_.find(room); counted_it != counted_room_to_node_.end()) {
        if (counted_it->second == node_id) {
            return;
        }

        auto prev_node_it = nodes_.find(counted_it->second);
        if (prev_node_it != nodes_.end()) {
            auto& prev_node = *prev_node_it->second;
            if (prev_node.active_rooms > 0) {
                prev_node.active_rooms--;
            }
            prev_node.update_load();
        }
    }

    auto& node = *it->second;
    node.active_rooms++;
    node.update_load();
    room_to_node_[room] = node_id;
    counted_room_to_node_[room] = node_id;
}

void LivekitNodeRegistry::decrement_room(const ChannelId& room) {
    std::lock_guard lock(mutex_);

    auto counted_it = counted_room_to_node_.find(room);
    if (counted_it == counted_room_to_node_.end()) return;

    auto it = nodes_.find(counted_it->second);
    if (it != nodes_.end()) {
        auto& node = *it->second;
        if (node.active_rooms > 0) node.active_rooms--;
        node.update_load();
    }

    counted_room_to_node_.erase(counted_it);
}

void LivekitNodeRegistry::increment_user(const std::string& node_id) {
    std::lock_guard lock(mutex_);

    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) return;

    auto& node = *it->second;
    node.active_users++;
    node.update_load();
}

void LivekitNodeRegistry::decrement_user(const std::string& node_id) {
    std::lock_guard lock(mutex_);

    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) return;

    auto& node = *it->second;

    if (node.active_users > 0) node.active_users--;

    node.update_load();
}

std::shared_ptr<const LivekitNode> LivekitNodeRegistry::pick_node() const {
    std::lock_guard lock(mutex_);

    std::shared_ptr<LivekitNode> best;
    double best_score = std::numeric_limits<double>::max();

    for (const auto& node_id : node_order_) {
        const auto it = nodes_.find(node_id);
        if (it == nodes_.end()) continue;

        const auto& node_ptr = it->second;
        if (!best || node_ptr->load_score < best_score) {
            best = node_ptr;
            best_score = node_ptr->load_score;
        }
    }

    return best;
}

std::vector<LivekitNode> LivekitNodeRegistry::list_nodes() const {
    std::lock_guard lock(mutex_);

    std::vector<LivekitNode> result;
    result.reserve(nodes_.size());

    for (const auto& node_id : node_order_) {
        const auto it = nodes_.find(node_id);
        if (it == nodes_.end()) continue;
        result.push_back(*it->second);
    }

    return result;
}

}  // namespace livekit
