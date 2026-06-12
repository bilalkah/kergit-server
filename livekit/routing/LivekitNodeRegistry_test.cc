#include "livekit/routing/LivekitNodeRegistry.h"

#include <gtest/gtest.h>

namespace livekit {
namespace {

TEST(LivekitNodeRegistryTest, BindRoomIsIdempotentAndDoesNotCountRoom) {
    LivekitNodeRegistry registry;
    registry.register_node({"node-a", "pub-a", "priv-a"});

    const ChannelId room{"00000000-0000-0000-0000-000000000001"};
    registry.bind_room(room, "node-a");
    registry.bind_room(room, "node-a");

    const auto bound = registry.get_room_node(room);
    ASSERT_NE(bound, nullptr);
    EXPECT_EQ(bound->node_id, "node-a");
    EXPECT_EQ(bound->active_rooms, 0U);
}

TEST(LivekitNodeRegistryTest, RebindMovesCountedRoomToNewNode) {
    LivekitNodeRegistry registry;
    registry.register_node({"node-a", "pub-a", "priv-a"});
    registry.register_node({"node-b", "pub-b", "priv-b"});

    const ChannelId room{"00000000-0000-0000-0000-000000000002"};
    registry.bind_room(room, "node-a");
    registry.increment_room(room, "node-a");

    auto node_a_before = registry.get_node("node-a");
    ASSERT_NE(node_a_before, nullptr);
    EXPECT_EQ(node_a_before->active_rooms, 1U);

    registry.bind_room(room, "node-b");

    const auto bound = registry.get_room_node(room);
    ASSERT_NE(bound, nullptr);
    EXPECT_EQ(bound->node_id, "node-b");

    auto node_a_after = registry.get_node("node-a");
    auto node_b_after = registry.get_node("node-b");
    ASSERT_NE(node_a_after, nullptr);
    ASSERT_NE(node_b_after, nullptr);
    EXPECT_EQ(node_a_after->active_rooms, 0U);
    EXPECT_EQ(node_b_after->active_rooms, 1U);
}

TEST(LivekitNodeRegistryTest, IncrementAndDecrementRoomAreIdempotentPerRoom) {
    LivekitNodeRegistry registry;
    registry.register_node({"node-a", "pub-a", "priv-a"});

    const ChannelId room{"00000000-0000-0000-0000-000000000003"};
    registry.bind_room(room, "node-a");

    registry.increment_room(room, "node-a");
    registry.increment_room(room, "node-a");

    auto node_after_increment = registry.get_node("node-a");
    ASSERT_NE(node_after_increment, nullptr);
    EXPECT_EQ(node_after_increment->active_rooms, 1U);

    registry.decrement_room(room);
    registry.decrement_room(room);

    auto node_after_decrement = registry.get_node("node-a");
    ASSERT_NE(node_after_decrement, nullptr);
    EXPECT_EQ(node_after_decrement->active_rooms, 0U);
}

TEST(LivekitNodeRegistryTest, ClearRoomDecrementsCountedRoomAndRemovesMapping) {
    LivekitNodeRegistry registry;
    registry.register_node({"node-a", "pub-a", "priv-a"});

    const ChannelId room{"00000000-0000-0000-0000-000000000004"};
    registry.bind_room(room, "node-a");
    registry.increment_room(room, "node-a");

    registry.clear_room(room);

    EXPECT_EQ(registry.get_room_node(room), nullptr);
    auto node = registry.get_node("node-a");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->active_rooms, 0U);
}

TEST(LivekitNodeRegistryTest, ListNodesPreservesRegistrationOrder) {
    LivekitNodeRegistry registry;
    registry.register_node({"node-a", "pub-a", "priv-a"});
    registry.register_node({"node-b", "pub-b", "priv-b"});
    registry.register_node({"node-c", "pub-c", "priv-c"});

    const auto nodes = registry.list_nodes();

    ASSERT_EQ(nodes.size(), 3U);
    EXPECT_EQ(nodes[0].node_id, "node-a");
    EXPECT_EQ(nodes[1].node_id, "node-b");
    EXPECT_EQ(nodes[2].node_id, "node-c");
}

TEST(LivekitNodeRegistryTest, PickNodeUsesRegistrationOrderAsTieBreaker) {
    LivekitNodeRegistry registry;
    registry.register_node({"node-a", "pub-a", "priv-a"});
    registry.register_node({"node-b", "pub-b", "priv-b"});

    const auto picked = registry.pick_node();

    ASSERT_NE(picked, nullptr);
    EXPECT_EQ(picked->node_id, "node-a");
}

}  // namespace
}  // namespace livekit
