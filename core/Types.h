#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include "App.h"

// Build-time switch (you can also wire this from Bazel with -DUSE_SSL=1)
#ifdef USE_SSL
static constexpr bool kSslEnabled = true;
#else
static constexpr bool kSslEnabled = false;
#endif

// OpCode alias so upper layers don't include uWS directly
using OpCode = uWS::OpCode;
using UwsApp = std::conditional_t<kSslEnabled, uWS::SSLApp, uWS::App>;

// Forward-declare PerSocketData (defined in net/PerSocketData.h)
struct PerSocketData;

template <typename T>
using UwsWebSocketT = uWS::WebSocket<kSslEnabled, true, T>;

#endif  // CORE_TYPES_H
