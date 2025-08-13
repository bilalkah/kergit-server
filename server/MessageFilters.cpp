#include "MessageFilters.h"

namespace {
std::function<void(json&)> g_incoming_filter;
std::function<void(json&)> g_outgoing_filter;
}

void set_incoming_filter(std::function<void(json&)> filter_fn) { g_incoming_filter = std::move(filter_fn); }

void set_outgoing_filter(std::function<void(json&)> filter_fn) { g_outgoing_filter = std::move(filter_fn); }

void apply_incoming_filter(json& message) {
	if (g_incoming_filter) {
		g_incoming_filter(message);
	}
}

void send_json(uWS::WebSocket<false, true, struct PerSocketData>* ws, json& message,
               uWS::OpCode op) {
	if (g_outgoing_filter) {
		g_outgoing_filter(message);
	}
	ws->send(message.dump(), op);
} 