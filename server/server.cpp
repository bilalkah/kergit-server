#include "ChatServerApp.h"
#include <iostream>

int main() {
    ChatServerApp server(9001);
    
    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    std::cout << "Chat server started. Press Ctrl+C to stop." << std::endl;
    
    // Keep the server running
    while (server.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}