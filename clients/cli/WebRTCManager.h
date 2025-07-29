#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// Include our WebRTC simple library
#include "third_party/webrtc/webrtc_simple.h"

enum class MediaType { VOICE, VIDEO, SCREEN_SHARE };

enum class WebRTCState { IDLE, CONNECTING, CONNECTED, DISCONNECTED, ERROR };

class WebRTCManager : public webrtc_simple::PeerConnectionObserver {
   public:
    WebRTCManager();
    ~WebRTCManager();

    // Call management
    bool initiateCall(const std::string& target_user, MediaType media_type);
    bool acceptCall(const std::string& call_id);
    bool rejectCall(const std::string& call_id);
    bool endCall(const std::string& call_id);

    // WebRTC signaling
    bool handleOffer(const std::string& call_id, const std::string& sdp);
    bool handleAnswer(const std::string& call_id, const std::string& sdp);
    bool handleIceCandidate(const std::string& call_id, const std::string& candidate);

    // Media stream management
    bool startLocalStream(MediaType media_type);
    bool stopLocalStream();
    bool startScreenShare();
    bool stopScreenShare();

    // State management
    WebRTCState getState() const;
    bool isInCall() const;
    std::string getCurrentCallId() const;

    // Event handlers
    void setOnCallStateChange(std::function<void(WebRTCState)> handler);
    void setOnRemoteStream(
        std::function<void(const std::string& user_id, webrtc_simple::MediaStream*)> handler);
    void setOnLocalStream(std::function<void(webrtc_simple::MediaStream*)> handler);
    void setOnIceCandidate(std::function<void(const std::string& candidate)> handler);
    void setOnOffer(std::function<void(const std::string& sdp)> handler);
    void setOnAnswer(std::function<void(const std::string& sdp)> handler);

    // Signaling callbacks (called by ChatClientApp)
    void onSignalingMessage(const std::string& type, const std::string& data);

    // PeerConnectionObserver implementation
    void OnConnectionChange(webrtc_simple::ConnectionState state) override;
    void OnIceConnectionChange(webrtc_simple::IceConnectionState state) override;
    void OnIceCandidate(const std::string& candidate) override;
    void OnAddStream(webrtc_simple::MediaStream* stream) override;
    void OnRemoveStream(webrtc_simple::MediaStream* stream) override;
    void OnDataChannel(const std::string& label) override;

   private:
    // WebRTC components
    std::unique_ptr<webrtc_simple::PeerConnection> peer_connection_;
    std::unique_ptr<webrtc_simple::MediaStream> local_stream_;
    std::unordered_map<std::string, webrtc_simple::MediaStream*> remote_streams_;

    WebRTCState current_state = WebRTCState::IDLE;
    std::string current_call_id;
    MediaType current_media_type = MediaType::VOICE;

    // Event handlers
    std::function<void(WebRTCState)> on_state_change;
    std::function<void(const std::string&, webrtc_simple::MediaStream*)> on_remote_stream;
    std::function<void(webrtc_simple::MediaStream*)> on_local_stream;
    std::function<void(const std::string&)> on_ice_candidate;
    std::function<void(const std::string&)> on_offer;
    std::function<void(const std::string&)> on_answer;

    // Helper methods
    void setState(WebRTCState new_state);
    bool createPeerConnection();
    void destroyPeerConnection();
    bool createLocalStream();
    void destroyLocalStream();
};