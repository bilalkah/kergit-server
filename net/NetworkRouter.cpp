#include "net/NetworkRouter.h"

#include <unordered_map>
#include <utility>
#include <vector>

namespace net {

void NetworkRouter::register_stack(std::unique_ptr<NetworkStack> stack) {
    auto id = stack->id();
    net_stacks_by_id_.emplace(id, std::move(stack));
}

outbound::PushResult NetworkRouter::push(outbound::OutgoingMessage msg) {
    // Fast path: with a single stack every recipient is on it, so forward the message
    // untouched — no per-stack regrouping map, no per-id copies, action moved not copied.
    if (net_stacks_by_id_.size() == 1) {
        return net_stacks_by_id_.begin()->second->outbound_sink().push(std::move(msg));
    }

    // Multi-stack: partition recipients by their owning stack, moving ids out of msg
    // (rather than copying each GlobalConnId).
    std::unordered_map<NetStackId, std::vector<GlobalConnId>> grouped;
    for (auto& cid : msg.target.conns) {
        grouped[cid.netstack_id].push_back(std::move(cid));
    }

    outbound::PushResult overall = outbound::PushResult::Ok;
    for (auto& [netstack_id, conns] : grouped) {
        auto it = net_stacks_by_id_.find(netstack_id);
        if (it == net_stacks_by_id_.end()) continue;

        outbound::OutgoingMessage forwarded{.priority = msg.priority,
                                            .target = outbound::Target{std::move(conns)},
                                            .action = msg.action};

        auto res = it->second->outbound_sink().push(std::move(forwarded));
        if (res == outbound::PushResult::DroppedHighPriority) {
            overall = res;
        } else if (res == outbound::PushResult::DroppedLowPriority &&
                   overall == outbound::PushResult::Ok) {
            overall = res;
        }
    }
    return overall;
}

void NetworkRouter::stop_all() {
    for (auto& [id, stack] : net_stacks_by_id_) {
        auto res = stack->stop();

        if (!res) {
            log(utils::LogLevel::ERROR, "Failed to stop NetworkStack " + id.value);
        } else {
            log(utils::LogLevel::WARN, "Stopped NetworkStack " + id.value);
        }
    }

    net_stacks_by_id_.clear();
}

void NetworkRouter::start_all() {
    for (auto& [id, stack] : net_stacks_by_id_) {
        auto res = stack->start();
        if (!res) {
            log(utils::LogLevel::ERROR, "Failed to start NetworkStack " + id.value);
        } else {
            log(utils::LogLevel::WARN, "Started NetworkStack " + id.value);
        }
    }
}

}  // namespace net
