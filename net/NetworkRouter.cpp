#include "net/NetworkRouter.h"

namespace net {

void NetworkRouter::register_stack(std::unique_ptr<NetworkStack> stack) {
    auto id = stack->id();
    net_stacks_by_id_.emplace(id, std::move(stack));
}

outbound::PushResult NetworkRouter::push(outbound::OutgoingMessage msg) {
    // group connections by netstack_id
    auto grouped = group_outgoing_msg(msg);
    outbound::PushResult overall = outbound::PushResult::Ok;

    // send once per netstack
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
        log(utils::LogLevel::WARN, "Stopping NetworkStack " + id.value);
        auto res = stack->stop();

        if (!res) {
            log(utils::LogLevel::ERROR, "Failed to stop NetworkStack " + id.value);
        } else {
            log(utils::LogLevel::INFO, "Stopped NetworkStack " + id.value);
        }
    }

    net_stacks_by_id_.clear();
}

void NetworkRouter::start_all() {
    for (auto& [id, stack] : net_stacks_by_id_) {
        log(utils::LogLevel::WARN, "Starting NetworkStack " + id.value);
        auto res = stack->start();
        if (!res) {
            log(utils::LogLevel::ERROR, "Failed to start NetworkStack " + id.value);
        } else {
            log(utils::LogLevel::INFO, "Started NetworkStack " + id.value);
        }
    }
}

std::unordered_map<NetStackId, std::vector<GlobalConnId>> NetworkRouter::group_outgoing_msg(
    const outbound::OutgoingMessage& msg) {
    std::unordered_map<NetStackId, std::vector<GlobalConnId>> grouped;

    for (const auto& cid : msg.target.conns) {
        grouped[cid.netstack_id].push_back(cid);
    }
    return grouped;
}

}  // namespace net
