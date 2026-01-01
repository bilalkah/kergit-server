#include "net/NetworkRouter.h"

namespace net {

void NetworkRouter::register_stack(std::unique_ptr<NetworkStack> stack) {
    auto id = stack->id();
    net_stacks_by_id_.emplace(id, std::move(stack));
    outbound_sinks_by_id_.emplace(id, net_stacks_by_id_[id]->outbound_sink());
}

void NetworkRouter::push(const outbound::OutgoingMessage& msg) {
    // group connections by netstack_id
    std::unordered_map<NetStackId, std::vector<GlobalConnId>> grouped;

    for (const auto& cid : msg.target.conns) {
        grouped[cid.netstack_id].push_back(cid);
    }

    // send once per netstack
    for (auto& [netstack_id, conns] : grouped) {
        auto it = outbound_sinks_by_id_.find(netstack_id);
        if (it == outbound_sinks_by_id_.end()) continue;

        outbound::OutgoingMessage forwarded{.target = outbound::Target{std::move(conns)},
                                            .action = msg.action};

        it->second.push(std::move(forwarded));
    }
}

void NetworkRouter::push(outbound::OutgoingMessage&& msg) {
    // group connections by netstack_id
    std::unordered_map<NetStackId, std::vector<GlobalConnId>> grouped;

    for (const auto& cid : msg.target.conns) {
        grouped[cid.netstack_id].push_back(cid);
    }

    // send once per netstack
    for (auto& [netstack_id, conns] : grouped) {
        auto it = outbound_sinks_by_id_.find(netstack_id);
        if (it == outbound_sinks_by_id_.end()) continue;

        outbound::OutgoingMessage forwarded{.target = outbound::Target{std::move(conns)},
                                            .action = msg.action};

        it->second.push(std::move(forwarded));
    }
}

}  // namespace net
