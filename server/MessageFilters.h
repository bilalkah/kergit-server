#pragma once
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

// Send a JSON message via uWS, applying the outgoing filter if set
void send_json(uWS::WebSocket<false, true, struct PerSocketData>* ws, json& message,
               uWS::OpCode op = uWS::OpCode::TEXT);