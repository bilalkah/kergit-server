#include "net/transport/websocket/utils.h"

using namespace sercom::protocol;

namespace net::transport::websocket {

std::string_view trim_ws(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string_view extract_token(std::string_view protocols) {
    auto comma = protocols.find(',');
    if (comma == std::string_view::npos) return {};

    auto protocol = trim_ws(protocols.substr(0, comma));
    if (protocol != "supabase") return {};

    return trim_ws(protocols.substr(comma + 1));
}

std::expected<std::string, std::string> make_app_pong_response(
    const sercom::protocol::Envelope& env) {
    if (env.type() != sercom::protocol::Envelope::PING) {
        return std::unexpected("Not a PING envelope");
    }

    sercom::protocol::system::Ping ping;
    if (!ping.ParseFromArray(env.payload().data(), env.payload().size())) {
        return std::unexpected("Invalid ping payload");
    }

    sercom::protocol::system::Pong pong;
    pong.set_client_ts_ms(ping.client_ts_ms());
    pong.set_server_ts_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count());

    sercom::protocol::Envelope out;
    out.set_version(1);
    out.set_type(sercom::protocol::Envelope::PONG);

    std::string pong_payload;
    pong.SerializeToString(&pong_payload);
    out.set_payload(std::move(pong_payload));

    std::string out_bytes;
    out.SerializeToString(&out_bytes);

    return out_bytes;
}

}  // namespace net::transport::websocket
