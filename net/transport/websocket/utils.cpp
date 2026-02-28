#include "net/transport/websocket/utils.h"

using namespace sercom::protocol;

namespace net::transport::websocket {

std::string_view app_pong_response_bytes() {
    static const std::string bytes = [] {
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
    }();

    return bytes;
}

}  // namespace net::transport::websocket
