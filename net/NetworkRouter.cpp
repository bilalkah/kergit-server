#include "net/NetworkRouter.h"

namespace net {

void NetworkRouter::register_stack(std::unique_ptr<NetworkStack> stack) {
    auto id = stack->id();
    net_stacks_by_id_.emplace(id, std::move(stack));
}

void NetworkRouter::push(const outbound::OutgoingMessage& msg) {
    for (const auto& cid : msg.target.conns) {
        auto it = net_stacks_by_id_.find(cid.netstack_id);
        if (it == net_stacks_by_id_.end()) continue;

        outbound::OutgoingMessage forwarded{.target = outbound::Target::one(cid),
                                            .action = msg.action};

        it->second->outbound_sink().push(std::move(forwarded));
    }
}

void NetworkRouter::push(outbound::OutgoingMessage&& msg) {
    for (const auto& cid : msg.target.conns) {
        auto it = net_stacks_by_id_.find(cid.netstack_id);
        if (it == net_stacks_by_id_.end()) continue;

        outbound::OutgoingMessage forwarded{.target = outbound::Target::one(cid),
                                            .action = std::move(msg.action)};

        it->second->outbound_sink().push(std::move(forwarded));
    }
}

}  // namespace net
