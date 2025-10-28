#pragma once

#include "App.h"

#include <string>

struct PerSocketData {
    std::string user_id;
    bool authenticated = false;
    
};

#ifdef USE_SSL
using WS = uWS::WebSocket<true, true, PerSocketData>;
#else
using WS = uWS::WebSocket<false, true, PerSocketData>;
#endif
