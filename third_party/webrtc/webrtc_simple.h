#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Simple WebRTC wrapper for voice calls
// This provides a clean interface that can be implemented with actual WebRTC later

namespace webrtc_simple {

// Forward declarations
class PeerConnection;
class MediaStream;
class AudioTrack;
class SessionDescription;

enum class MediaType {
    AUDIO,
    VIDEO,
    SCREEN_SHARE
};

enum class ConnectionState {
    NEW,
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    FAILED
};

enum class IceConnectionState {
    NEW,
    CHECKING,
    CONNECTED,
    COMPLETED,
    FAILED,
    DISCONNECTED,
    CLOSED
};

// Callback interfaces
class PeerConnectionObserver {
public:
    virtual ~PeerConnectionObserver() = default;
    
    virtual void OnConnectionChange(ConnectionState state) = 0;
    virtual void OnIceConnectionChange(IceConnectionState state) = 0;
    virtual void OnIceCandidate(const std::string& candidate) = 0;
    virtual void OnAddStream(MediaStream* stream) = 0;
    virtual void OnRemoveStream(MediaStream* stream) = 0;
    virtual void OnDataChannel(const std::string& label) = 0;
};

class MediaStreamObserver {
public:
    virtual ~MediaStreamObserver() = default;
    
    virtual void OnAddTrack(AudioTrack* track) = 0;
    virtual void OnRemoveTrack(AudioTrack* track) = 0;
};

// Configuration structures
struct RTCConfiguration {
    std::vector<std::string> ice_servers;
    bool ice_transport_policy = false;
    bool bundle_policy = false;
    bool rtcp_mux_policy = false;
    bool tcp_candidate_policy = false;
    bool candidate_network_policy = false;
    int audio_jitter_buffer_max_packets = 50;
    bool audio_jitter_buffer_fast_accelerate = false;
    int ice_connection_receiving_timeout = 0;
    int ice_backup_candidate_pair_ping_interval = 0;
    bool key_type = false;
    bool continual_gathering_policy = false;
    bool disable_ipv6_on_wifi = false;
    int max_ipv6_networks = 0;
    bool disable_link_local_networks = false;
    bool enable_rtp_data_channel = false;
    bool screencast_min_bitrate = false;
    bool combined_audio_video_bwe = false;
    bool enable_dtls_srtp = false;
};

struct MediaConstraints {
    std::vector<std::pair<std::string, std::string>> mandatory;
    std::vector<std::pair<std::string, std::string>> optional;
};

// Main classes
class AudioTrack {
public:
    virtual ~AudioTrack() = default;
    
    virtual std::string id() const = 0;
    virtual bool enabled() const = 0;
    virtual void set_enabled(bool enabled) = 0;
    virtual MediaType kind() const = 0;
};

class MediaStream {
public:
    virtual ~MediaStream() = default;
    
    virtual std::string id() const = 0;
    virtual std::vector<AudioTrack*> GetAudioTracks() = 0;
    virtual void AddTrack(AudioTrack* track) = 0;
    virtual void RemoveTrack(AudioTrack* track) = 0;
    virtual void set_observer(MediaStreamObserver* observer) = 0;
};

class SessionDescription {
public:
    enum class Type {
        OFFER,
        ANSWER,
        PRANSWER,
        ROLLBACK
    };
    
    virtual ~SessionDescription() = default;
    
    virtual Type type() const = 0;
    virtual std::string sdp() const = 0;
    virtual void set_sdp(const std::string& sdp) = 0;
    virtual void set_type(Type type) = 0;
};

class PeerConnection {
public:
    virtual ~PeerConnection() = default;
    
    // Connection management
    virtual bool Initialize(const RTCConfiguration& config) = 0;
    virtual void Close() = 0;
    
    // Media streams
    virtual bool AddStream(MediaStream* stream) = 0;
    virtual void RemoveStream(MediaStream* stream) = 0;
    
    // Session description
    virtual bool CreateOffer(SessionDescription* offer, const MediaConstraints& constraints) = 0;
    virtual bool CreateAnswer(SessionDescription* answer, const MediaConstraints& constraints) = 0;
    virtual bool SetLocalDescription(SessionDescription* desc) = 0;
    virtual bool SetRemoteDescription(SessionDescription* desc) = 0;
    
    // ICE candidates
    virtual bool AddIceCandidate(const std::string& candidate) = 0;
    
    // Observers
    virtual void set_observer(PeerConnectionObserver* observer) = 0;
    
    // State queries
    virtual ConnectionState connection_state() const = 0;
    virtual IceConnectionState ice_connection_state() const = 0;
};

// Factory functions
std::unique_ptr<PeerConnection> CreatePeerConnection(const RTCConfiguration& config);
std::unique_ptr<SessionDescription> CreateSessionDescription(SessionDescription::Type type, const std::string& sdp);
std::unique_ptr<MediaStream> CreateMediaStream(const std::string& id);
std::unique_ptr<AudioTrack> CreateAudioTrack(const std::string& id);

} // namespace webrtc_simple 