#include "client/WebRTCManager.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    std::cout << "=== Voice Call Test ===" << std::endl;

    // Create WebRTC manager
    WebRTCManager webrtc_manager;

    // Set up event handlers
    webrtc_manager.setOnCallStateChange([](WebRTCState state) {
        std::cout << "[CALL] State changed: ";
        switch (state) {
            case WebRTCState::IDLE:
                std::cout << "IDLE";
                break;
            case WebRTCState::CONNECTING:
                std::cout << "CONNECTING";
                break;
            case WebRTCState::CONNECTED:
                std::cout << "CONNECTED";
                break;
            case WebRTCState::DISCONNECTED:
                std::cout << "DISCONNECTED";
                break;
            case WebRTCState::ERROR:
                std::cout << "ERROR";
                break;
        }
        std::cout << std::endl;
    });

    webrtc_manager.setOnLocalStream([](webrtc_simple::MediaStream* stream) {
        std::cout << "[CALL] Local stream ready: " << stream->id() << std::endl;
    });

    webrtc_manager.setOnRemoteStream([](const std::string& user_id,
                                        webrtc_simple::MediaStream* stream) {
        std::cout << "[CALL] Remote stream from " << user_id << ": " << stream->id() << std::endl;
    });

    webrtc_manager.setOnIceCandidate([](const std::string& candidate) {
        std::cout << "[CALL] ICE candidate: " << candidate << std::endl;
    });

    webrtc_manager.setOnOffer([](const std::string& sdp) {
        std::cout << "[CALL] Generated offer SDP:" << std::endl;
        std::cout << sdp << std::endl;
    });

    webrtc_manager.setOnAnswer([](const std::string& sdp) {
        std::cout << "[CALL] Generated answer SDP:" << std::endl;
        std::cout << sdp << std::endl;
    });

    // Test 1: Initiate a voice call
    std::cout << "\n--- Test 1: Initiate Voice Call ---" << std::endl;
    if (webrtc_manager.initiateCall("alice", MediaType::VOICE)) {
        std::cout << "Voice call initiated successfully!" << std::endl;
    } else {
        std::cout << "Failed to initiate voice call!" << std::endl;
        return 1;
    }

    // Wait a bit for the call to establish
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 2: Simulate receiving an offer (in a real scenario, this would come from signaling)
    std::cout << "\n--- Test 2: Handle Incoming Offer ---" << std::endl;
    std::string test_offer =
        "v=0\r\n"
        "o=- 1234567890 2 IN IP4 127.0.0.1\r\n"
        "s=-\r\n"
        "t=0 0\r\n"
        "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "a=mid:audio\r\n"
        "a=sendrecv\r\n"
        "a=rtpmap:111 opus/48000/2\r\n";

    if (webrtc_manager.handleOffer("test_call_123", test_offer)) {
        std::cout << "Offer handled successfully!" << std::endl;
    } else {
        std::cout << "Failed to handle offer!" << std::endl;
    }

    // Wait for the call to connect
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Test 3: Check call status
    std::cout << "\n--- Test 3: Call Status ---" << std::endl;
    std::cout << "Is in call: " << (webrtc_manager.isInCall() ? "Yes" : "No") << std::endl;
    std::cout << "Current call ID: " << webrtc_manager.getCurrentCallId() << std::endl;
    std::cout << "Call state: " << static_cast<int>(webrtc_manager.getState()) << std::endl;

    // Test 4: End the call
    std::cout << "\n--- Test 4: End Call ---" << std::endl;
    if (webrtc_manager.endCall("test_call_123")) {
        std::cout << "Call ended successfully!" << std::endl;
    } else {
        std::cout << "Failed to end call!" << std::endl;
    }

    // Test 5: Accept a call
    std::cout << "\n--- Test 5: Accept Call ---" << std::endl;
    if (webrtc_manager.acceptCall("incoming_call_456")) {
        std::cout << "Call accepted successfully!" << std::endl;
    } else {
        std::cout << "Failed to accept call!" << std::endl;
    }

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // End the second call
    webrtc_manager.endCall("incoming_call_456");

    std::cout << "\n=== Voice Call Test Complete ===" << std::endl;
    return 0;
}