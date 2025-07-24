#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <string>

using websocketpp::connection_hdl;
typedef websocketpp::client<websocketpp::config::asio_client> client;

void on_message(client* c, connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg) {
    std::cout << "Received: " << msg->get_payload() << std::endl;
}

int main() {
    client c;
    std::string uri = "ws://localhost:9001";

    try {
        c.set_access_channels(websocketpp::log::alevel::none);
        c.clear_access_channels(websocketpp::log::alevel::frame_payload);
        c.init_asio();
        c.set_message_handler(std::bind(&on_message, &c, std::placeholders::_1, std::placeholders::_2));

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection(uri, ec);
        if (ec) {
            std::cout << "Could not create connection: " << ec.message() << std::endl;
            return 1;
        }

        c.connect(con);

        // Run the ASIO io_service on a separate thread
        std::thread t([&c]() { c.run(); });

        // Send a message from stdin
        std::string line;
        while (std::getline(std::cin, line)) {
            con->send(line);
        }

        t.join();
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    return 0;
} 