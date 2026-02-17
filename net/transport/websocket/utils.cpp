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

std::optional<std::string> extract_supabase_token(const std::string& header) {
    static constexpr std::string_view prefix = "supabase ";
    if (header.size() < prefix.size()) {
        return std::nullopt;
    }
    if (header.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }
    auto token = header.substr(prefix.size());
    if (token.empty()) {
        return std::nullopt;
    }
    return token;
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
