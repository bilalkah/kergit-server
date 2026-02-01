#include "net/connection/ConnectionRegistery.h"

namespace net::connection {

void ConnectionRegistery::attach(const ConnId& conn_id, ConnectionContext context) {
    std::unique_lock lock(mutex_);
    connections_.emplace(conn_id, std::move(context));
}

void ConnectionRegistery::detach(const ConnId& conn_id) {
    std::unique_lock lock(mutex_);
    connections_.erase(conn_id);
}

ConnectionResult ConnectionRegistery::get(const ConnId& conn_id) const {
    std::shared_lock lock(mutex_);
    auto it = connections_.find(conn_id);
    if (it != connections_.end()) {
        return it->second;
    } else {
        return std::unexpected(ConnectionError{"Connection ID not found"});
    }
}

std::vector<ConnectionResult> ConnectionRegistery::get(
    const std::vector<GlobalConnId>& global_ids) const {
    std::shared_lock lock(mutex_);
    std::vector<ConnectionResult> result;
    result.reserve(global_ids.size());
    for (const auto& global_id : global_ids) {
        auto it = connections_.find(global_id.conn_id);
        if (it != connections_.end()) {
            result.push_back(it->second);
        } else {
            result.push_back(std::unexpected(ConnectionError{"Connection ID not found"}));
        }
    }
    return result;
}

std::vector<ConnectionResult> ConnectionRegistery::get() const {
    std::shared_lock lock(mutex_);
    std::vector<ConnectionResult> result;
    result.reserve(connections_.size());
    for (const auto& [conn_id, context] : connections_) {
        result.push_back(context);
    }
    return result;
}

std::optional<ConnectionView> ConnectionRegistery::get_view(const ConnId& conn_id) const {
    // Hot-path: returns a snapshot only; no containers are copied.
    std::shared_lock lock(mutex_);
    auto it = connections_.find(conn_id);
    if (it == connections_.end()) {
        return std::nullopt;
    }
    const auto& ctx = it->second;
    return ConnectionView{.conn_id = ctx.conn_id,
                          .handle = ctx.handle,
                          .kind = ctx.kind,
                          .auth = ctx.auth};
}

std::vector<ConnId> ConnectionRegistery::get_ids() const {
    std::shared_lock lock(mutex_);
    std::vector<ConnId> ids;
    ids.reserve(connections_.size());
    for (const auto& [conn_id, ctx] : connections_) {
        ids.push_back(conn_id);
    }
    return ids;
}

std::optional<OutboundReady> ConnectionRegistery::take_one_outbound(const ConnId& conn_id) {
    std::unique_lock lock(mutex_);
    auto it = connections_.find(conn_id);
    if (it == connections_.end()) {
        return std::nullopt;
    }
    auto& ctx = it->second;
    if (ctx.outbox.drop_pending) {
        ctx.outbox.drop_pending = false;
        ctx.outbox.slow_hits = 0;
        ctx.outbox.q.clear();
        return OutboundReady{.handle = ctx.handle, .msg = {}, .drop_pending = true};
    }
    if (ctx.outbox.q.empty()) {
        return std::nullopt;
    }
    OutboundReady ready;
    ready.handle = ctx.handle;
    ready.msg = std::move(ctx.outbox.q.front());
    ctx.outbox.q.pop_front();
    return ready;
}

std::expected<void, ConnectionError> ConnectionRegistery::mutate(
    const ConnId& conn_id, const std::function<void(ConnectionContext&)>& mutator) {
    std::unique_lock lock(mutex_);
    auto it = connections_.find(conn_id);
    if (it != connections_.end()) {
        mutator(it->second);
        return {};
    } else {
        return std::unexpected(ConnectionError{"Connection ID not found"});
    }
}

size_t ConnectionRegistery::size() const {
    std::shared_lock lock(mutex_);
    return connections_.size();
}

}  // namespace net::connection
