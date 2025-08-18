#pragma once
#include "server/Config.h"

#include <App.h>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Set a filter that can mutate incoming messages right after parsing
void set_incoming_filter(std::function<void(json&)> filter_fn);

// Set a filter that can mutate outgoing messages just before send
void set_outgoing_filter(std::function<void(json&)> filter_fn);

// Apply the incoming filter if set
void apply_incoming_filter(json& message);

// Generic over SSL bool; header-only so it links.
template <bool SSL>
inline void send_json(uWS::WebSocket<SSL, true, PerSocketData>* ws, json& message,
                      uWS::OpCode opcode = uWS::OpCode::TEXT) {
    // (optionally guard backpressure here if you want)
    ws->send(message.dump(), opcode);
}
