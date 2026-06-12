#include "livekit/cli/LivekitClient.h"

#include <cctype>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

namespace livekit::cli {
namespace {

constexpr int kConnectTimeoutMs = 2000;
constexpr int kRequestTimeoutMs = 5000;
constexpr std::size_t kMaxErrorBodyLength = 512;

std::string summarize_error_body(std::string body) {
    for (char& character : body) {
        if (std::isspace(static_cast<unsigned char>(character))) {
            character = ' ';
        }
    }
    if (body.size() > kMaxErrorBodyLength) {
        body.resize(kMaxErrorBodyLength);
        body += "...";
    }
    return body;
}

}  // namespace

LivekitClient::LivekitClient(std::string host, const livekit::LiveKitTokenService& token_service)
    : host_(std::move(host)), token_service_(token_service) {}

std::string LivekitClient::PostJson(const std::string& endpoint, const std::string& body,
                                    const std::string& token) const {
    const std::string url = host_ + endpoint;

    auto response = cpr::Post(
        cpr::Url{url}, cpr::Body{body},
        cpr::Header{{"Authorization", "Bearer " + token}, {"Content-Type", "application/json"}},
        cpr::ConnectTimeout{kConnectTimeoutMs}, cpr::Timeout{kRequestTimeoutMs});

    if (response.error) {
        throw std::runtime_error("LiveKit request failed: endpoint=" + endpoint + " url=" + url +
                                 " error=" + response.error.message);
    }

    if (response.status_code >= 400) {
        throw std::runtime_error("LiveKit HTTP/Twirp error: endpoint=" + endpoint + " url=" + url +
                                 " status=" + std::to_string(response.status_code) +
                                 " twirp=" + summarize_error_body(response.text));
    }

    return response.text;
}

std::vector<ParticipantInfo> LivekitClient::ListParticipants(const ChannelId& room) {
    json req{{"room", room.value}};
    livekit::LiveKitTokenService::AdminTokenRequest token_req;
    token_req.room = room;
    token_req.room_admin = true;
    const std::string token = token_service_.mint_admin_token(token_req);

    auto res = PostJson("/twirp/livekit.RoomService/ListParticipants", req.dump(), token);
    json data = json::parse(res);

    std::vector<ParticipantInfo> out;
    if (!data.contains("participants")) return out;

    for (const auto& p : data["participants"]) {
        ParticipantInfo info;
        info.sid = p.value("sid", "");
        info.identity = UserId(p.value("identity", ""));
        info.metadata = p.value("metadata", "");
        info.is_publisher = p.value("isPublisher", false);
        out.push_back(info);
    }

    return out;
}

bool LivekitClient::RemoveParticipant(const ChannelId& room, const UserId& identity) {
    try {
        json req{{"room", room.value}, {"identity", identity.value}};
        livekit::LiveKitTokenService::AdminTokenRequest token_req;
        token_req.room = room;
        token_req.room_admin = true;
        const std::string token = token_service_.mint_admin_token(token_req);
        PostJson("/twirp/livekit.RoomService/RemoveParticipant", req.dump(), token);
        return true;
    } catch (...) {
        return false;
    }
}

void LivekitClient::CreateRoom(const std::string& name, const std::string& metadata) {
    livekit::LiveKitTokenService::AdminTokenRequest token_req;
    token_req.room_create = true;
    const std::string token = token_service_.mint_admin_token(token_req);
    json req{{"name", name}, {"emptyTimeout", 60}};
    if (!metadata.empty()) req["metadata"] = metadata;
    PostJson("/twirp/livekit.RoomService/CreateRoom", req.dump(), token);
}

void LivekitClient::DeleteRoom(const std::string& name) {
    livekit::LiveKitTokenService::AdminTokenRequest token_req;
    token_req.room_create = true;
    const std::string token = token_service_.mint_admin_token(token_req);
    json req{{"room", name}};
    PostJson("/twirp/livekit.RoomService/DeleteRoom", req.dump(), token);
}

std::vector<ChannelId> LivekitClient::ListRooms() {
    livekit::LiveKitTokenService::AdminTokenRequest token_req;
    token_req.room_list = true;
    const std::string token = token_service_.mint_admin_token(token_req);
    const auto res = PostJson("/twirp/livekit.RoomService/ListRooms", "{}", token);
    const auto data = json::parse(res);

    std::vector<ChannelId> rooms;
    if (data.contains("rooms")) {
        for (const auto& r : data["rooms"]) {
            const std::string name = r.value("name", "");
            if (!name.empty()) rooms.emplace_back(name);
        }
    }
    return rooms;
}

}  // namespace livekit::cli
