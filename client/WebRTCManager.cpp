#include "WebRTCManager.h"

#include <algorithm>
#include <iostream>

WebRTCManager::WebRTCManager() {
    std::cout << "[WebRTC] Manager initialized with WebRTC simple library" << std::endl;
}

WebRTCManager::~WebRTCManager() {
    destroyPeerConnection();
    destroyLocalStream();
}

bool WebRTCManager::initiateCall(const std::string& target_user, MediaType media_type) {
    std::cout << "[WebRTC] Initiating " << (media_type == MediaType::VOICE ? "voice" : "video")
              << " call to " << target_user << std::endl;

    current_media_type = media_type;
    setState(WebRTCState::CONNECTING);

    // Create peer connection
    if (!createPeerConnection()) {
        std::cerr << "[WebRTC] Failed to create peer connection" << std::endl;
        setState(WebRTCState::ERROR);
        return false;
    }

    // Create local stream
    if (!createLocalStream()) {
        std::cerr << "[WebRTC] Failed to create local stream" << std::endl;
        setState(WebRTCState::ERROR);
        return false;
    }

    // Add local stream to peer connection
    if (peer_connection_ && local_stream_) {
        peer_connection_->AddStream(local_stream_.get());
    }

    return true;
}

bool WebRTCManager::acceptCall(const std::string& call_id) {
    std::cout << "[WebRTC] Accepting call " << call_id << std::endl;

    current_call_id = call_id;
    setState(WebRTCState::CONNECTING);

    // Create peer connection
    if (!createPeerConnection()) {
        std::cerr << "[WebRTC] Failed to create peer connection" << std::endl;
        setState(WebRTCState::ERROR);
        return false;
    }

    // Create local stream
    if (!createLocalStream()) {
        std::cerr << "[WebRTC] Failed to create local stream" << std::endl;
        setState(WebRTCState::ERROR);
        return false;
    }

    // Add local stream to peer connection
    if (peer_connection_ && local_stream_) {
        peer_connection_->AddStream(local_stream_.get());
    }

    return true;
}

bool WebRTCManager::rejectCall(const std::string& call_id) {
    std::cout << "[WebRTC] Rejecting call " << call_id << std::endl;

    destroyPeerConnection();
    setState(WebRTCState::IDLE);
    return true;
}

bool WebRTCManager::endCall(const std::string& call_id) {
    std::cout << "[WebRTC] Ending call " << call_id << std::endl;

    destroyPeerConnection();
    destroyLocalStream();
    setState(WebRTCState::IDLE);
    return true;
}

bool WebRTCManager::handleOffer(const std::string& call_id, const std::string& sdp) {
    std::cout << "[WebRTC] Handling offer for call " << call_id << std::endl;

    if (!peer_connection_) {
        std::cerr << "[WebRTC] No peer connection available" << std::endl;
        return false;
    }

    auto session_desc = webrtc_simple::CreateSessionDescription(
        webrtc_simple::SessionDescription::Type::OFFER, sdp);

    if (!session_desc) {
        std::cerr << "[WebRTC] Failed to create session description" << std::endl;
        return false;
    }

    // Set remote description
    if (!peer_connection_->SetRemoteDescription(session_desc.get())) {
        std::cerr << "[WebRTC] Failed to set remote description" << std::endl;
        return false;
    }

    // Create answer
    webrtc_simple::MediaConstraints constraints;
    auto answer = webrtc_simple::CreateSessionDescription(
        webrtc_simple::SessionDescription::Type::ANSWER, "");

    if (!peer_connection_->CreateAnswer(answer.get(), constraints)) {
        std::cerr << "[WebRTC] Failed to create answer" << std::endl;
        return false;
    }

    // Set local description
    if (!peer_connection_->SetLocalDescription(answer.get())) {
        std::cerr << "[WebRTC] Failed to set local description" << std::endl;
        return false;
    }

    // Send answer through signaling
    if (on_answer) {
        on_answer(answer->sdp());
    }

    return true;
}

bool WebRTCManager::handleAnswer(const std::string& call_id, const std::string& sdp) {
    std::cout << "[WebRTC] Handling answer for call " << call_id << std::endl;

    if (!peer_connection_) {
        std::cerr << "[WebRTC] No peer connection available" << std::endl;
        return false;
    }

    auto session_desc = webrtc_simple::CreateSessionDescription(
        webrtc_simple::SessionDescription::Type::ANSWER, sdp);

    if (!session_desc) {
        std::cerr << "[WebRTC] Failed to create session description" << std::endl;
        return false;
    }

    // Set remote description
    if (!peer_connection_->SetRemoteDescription(session_desc.get())) {
        std::cerr << "[WebRTC] Failed to set remote description" << std::endl;
        return false;
    }

    return true;
}

bool WebRTCManager::handleIceCandidate(const std::string& call_id, const std::string& candidate) {
    std::cout << "[WebRTC] Handling ICE candidate for call " << call_id << std::endl;

    if (!peer_connection_) {
        std::cerr << "[WebRTC] No peer connection available" << std::endl;
        return false;
    }

    if (!peer_connection_->AddIceCandidate(candidate)) {
        std::cerr << "[WebRTC] Failed to add ICE candidate" << std::endl;
        return false;
    }

    return true;
}

bool WebRTCManager::startLocalStream(MediaType media_type) {
    std::cout << "[WebRTC] Starting local " << (media_type == MediaType::VOICE ? "voice" : "video")
              << " stream" << std::endl;

    current_media_type = media_type;
    return createLocalStream();
}

bool WebRTCManager::stopLocalStream() {
    std::cout << "[WebRTC] Stopping local stream" << std::endl;
    destroyLocalStream();
    return true;
}

bool WebRTCManager::startScreenShare() {
    std::cout << "[WebRTC] Starting screen share" << std::endl;
    return true;
}

bool WebRTCManager::stopScreenShare() {
    std::cout << "[WebRTC] Stopping screen share" << std::endl;
    return true;
}

WebRTCState WebRTCManager::getState() const { return current_state; }

bool WebRTCManager::isInCall() const {
    return current_state == WebRTCState::CONNECTING || current_state == WebRTCState::CONNECTED;
}

std::string WebRTCManager::getCurrentCallId() const { return current_call_id; }

void WebRTCManager::setOnCallStateChange(std::function<void(WebRTCState)> handler) {
    on_state_change = handler;
}

void WebRTCManager::setOnRemoteStream(
    std::function<void(const std::string&, webrtc_simple::MediaStream*)> handler) {
    on_remote_stream = handler;
}

void WebRTCManager::setOnLocalStream(std::function<void(webrtc_simple::MediaStream*)> handler) {
    on_local_stream = handler;
}

void WebRTCManager::setOnIceCandidate(std::function<void(const std::string&)> handler) {
    on_ice_candidate = handler;
}

void WebRTCManager::setOnOffer(std::function<void(const std::string&)> handler) {
    on_offer = handler;
}

void WebRTCManager::setOnAnswer(std::function<void(const std::string&)> handler) {
    on_answer = handler;
}

void WebRTCManager::onSignalingMessage(const std::string& type, const std::string& data) {
    std::cout << "[WebRTC] Received signaling message: " << type << std::endl;

    if (type == "webrtc_signal") {
        // Parse the signaling message and handle accordingly
        // This would be implemented based on your signaling protocol
    }
}

// PeerConnectionObserver implementation
void WebRTCManager::OnConnectionChange(webrtc_simple::ConnectionState state) {
    std::cout << "[WebRTC] Connection state changed: " << static_cast<int>(state) << std::endl;

    switch (state) {
        case webrtc_simple::ConnectionState::CONNECTED:
            setState(WebRTCState::CONNECTED);
            break;
        case webrtc_simple::ConnectionState::DISCONNECTED:
        case webrtc_simple::ConnectionState::FAILED:
            setState(WebRTCState::DISCONNECTED);
            break;
        case webrtc_simple::ConnectionState::CONNECTING:
            setState(WebRTCState::CONNECTING);
            break;
        default:
            break;
    }
}

void WebRTCManager::OnIceConnectionChange(webrtc_simple::IceConnectionState state) {
    std::cout << "[WebRTC] ICE connection state changed: " << static_cast<int>(state) << std::endl;
}

void WebRTCManager::OnIceCandidate(const std::string& candidate) {
    std::cout << "[WebRTC] ICE candidate generated: " << candidate << std::endl;

    if (on_ice_candidate) {
        on_ice_candidate(candidate);
    }
}

void WebRTCManager::OnAddStream(webrtc_simple::MediaStream* stream) {
    std::cout << "[WebRTC] Remote stream added: " << stream->id() << std::endl;

    remote_streams_[stream->id()] = stream;

    if (on_remote_stream) {
        on_remote_stream(stream->id(), stream);
    }
}

void WebRTCManager::OnRemoveStream(webrtc_simple::MediaStream* stream) {
    std::cout << "[WebRTC] Remote stream removed: " << stream->id() << std::endl;

    remote_streams_.erase(stream->id());
}

void WebRTCManager::OnDataChannel(const std::string& label) {
    std::cout << "[WebRTC] Data channel opened: " << label << std::endl;
}

void WebRTCManager::setState(WebRTCState new_state) {
    if (current_state != new_state) {
        current_state = new_state;
        if (on_state_change) {
            on_state_change(new_state);
        }
    }
}

bool WebRTCManager::createPeerConnection() {
    webrtc_simple::RTCConfiguration config;

    // Add STUN servers
    config.ice_servers.push_back("stun:stun.l.google.com:19302");
    config.ice_servers.push_back("stun:stun1.l.google.com:19302");

    peer_connection_ = webrtc_simple::CreatePeerConnection(config);

    if (!peer_connection_) {
        std::cerr << "[WebRTC] Failed to create peer connection" << std::endl;
        return false;
    }

    // Set ourselves as the observer
    peer_connection_->set_observer(this);

    std::cout << "[WebRTC] Peer connection created successfully" << std::endl;
    return true;
}

void WebRTCManager::destroyPeerConnection() {
    if (peer_connection_) {
        peer_connection_->Close();
        peer_connection_.reset();
        std::cout << "[WebRTC] Peer connection destroyed" << std::endl;
    }
}

bool WebRTCManager::createLocalStream() {
    // Create a simple local stream with audio track
    local_stream_ = webrtc_simple::CreateMediaStream("local_stream");

    // Add an audio track to the stream
    auto audio_track = webrtc_simple::CreateAudioTrack("audio_track");
    local_stream_->AddTrack(audio_track.get());

    // Store the track (in real implementation, you'd manage this properly)
    // For now, we'll just create it and let it be managed by the stream

    if (on_local_stream) {
        on_local_stream(local_stream_.get());
    }

    std::cout << "[WebRTC] Local stream created successfully" << std::endl;
    return true;
}

void WebRTCManager::destroyLocalStream() {
    if (local_stream_) {
        local_stream_.reset();
        std::cout << "[WebRTC] Local stream destroyed" << std::endl;
    }
}