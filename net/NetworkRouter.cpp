#include "net/NetworkRouter.h"

namespace net {

void NetworkRouter::register_stack(std::unique_ptr<NetworkStack> stack) {
    auto id = stack->id();
    net_stacks_by_id_.emplace(id, std::move(stack));
}

void NetworkRouter::push(const outbound::OutgoingMessage& msg) {
    // group connections by netstack_id
    std::unordered_map<NetStackId, std::vector<GlobalConnId>> grouped;

    for (const auto& cid : msg.target.conns) {
        grouped[cid.netstack_id].push_back(cid);
    }

    // send once per netstack
    for (auto& [netstack_id, conns] : grouped) {
        auto it = net_stacks_by_id_.find(netstack_id);
        if (it == net_stacks_by_id_.end()) continue;

        outbound::OutgoingMessage forwarded{.target = outbound::Target{std::move(conns)},
                                            .action = msg.action};

        it->second->outbound_sink().push(std::move(forwarded));
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
        auto it = net_stacks_by_id_.find(netstack_id);
        if (it == net_stacks_by_id_.end()) continue;

        outbound::OutgoingMessage forwarded{.target = outbound::Target{std::move(conns)},
                                            .action = msg.action};

        it->second->outbound_sink().push(std::move(forwarded));
    }
}

void NetworkRouter::stop_all() {
    for (auto& [id, stack] : net_stacks_by_id_) {
        stack->stop();
    }
}

void NetworkRouter::start_all() {
    for (auto& [id, stack] : net_stacks_by_id_) {
        stack->start();
        outbound_sinks_by_id_.emplace(id, stack->outbound_sink());
    }
}

}  // namespace net
