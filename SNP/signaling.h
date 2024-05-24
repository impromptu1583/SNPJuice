#pragma once
#include "common.h"
#include "JuiceManager.h"
#include "config.h"

enum class SignalMessageType {
	StartAdvertising = 1,
	StopAdvertising,
	RequestAdvertisers,
	SolicitAds,
	GameAd,

	JuiceLocalDescription = 101,
	JuciceCandidate,
	JuiceDone,

	ServerSetID = 254,
	ServerEcho = 255,
};

inline std::string to_string(SignalMessageType value) {
	switch (value) {
		EnumStringCase(SignalMessageType::StartAdvertising);
		EnumStringCase(SignalMessageType::StopAdvertising);
		EnumStringCase(SignalMessageType::RequestAdvertisers);
		EnumStringCase(SignalMessageType::SolicitAds);
		EnumStringCase(SignalMessageType::GameAd);

		EnumStringCase(SignalMessageType::JuiceLocalDescription);
		EnumStringCase(SignalMessageType::JuciceCandidate);
		EnumStringCase(SignalMessageType::JuiceDone);

		EnumStringCase(SignalMessageType::ServerSetID);
		EnumStringCase(SignalMessageType::ServerEcho);
	}
	return std::to_string((s32)value);
}

enum class SocketState {
	Uninitialized,
	Initialized,
	Connecting,
	Ready
};

inline std::string to_string(SocketState value) {
	switch (value) {
		EnumStringCase(SocketState::Uninitialized);
		EnumStringCase(SocketState::Initialized);
		EnumStringCase(SocketState::Connecting);
		EnumStringCase(SocketState::Ready);
	}
	return std::to_string((s32)value);
}

struct SignalPacket {
	SNetAddr peer_id;
	SignalMessageType message_type;
	std::string data;

	SignalPacket() = default;

	SignalPacket(SNetAddr ID, SignalMessageType type, std::string d)
		: peer_id{ID}, message_type{type}, data{d} {}
	
	SignalPacket(std::string& packet_string) {
		memcpy_s((void*)&peer_id.address, sizeof(peer_id.address), packet_string.c_str(), sizeof(SNetAddr));
		message_type = SignalMessageType((int)packet_string.at(16) - 48);
		std::cout << (int)message_type << "msg type\n";
		data = packet_string.substr(sizeof(SNetAddr) + 1);
	}
};

class SignalingSocket {
public:
	SignalingSocket() = default;
	SignalingSocket(SignalingSocket&) = delete;
	SignalingSocket& operator=(SignalingSocket&) = delete;

	~SignalingSocket() {
		deinitialize();
	}

	bool initialize();
	void deinitialize();
	void send_packet(SNetAddr dest, SignalMessageType msg_type, const std::string& msg = "");
	void send_packet(const SignalPacket& packet);
	int  receive_packets(std::vector<SignalPacket>& incoming_packets);
	void start_advertising();
	void stop_advertising();
	void request_advertisers();
	void echo(std::string data);
	void set_client_id(std::string id);
	
private:
	void split_into_packets(const std::string& s, std::vector<SignalPacket>& incoming_packets);

private:
	SNetAddr m_server{};
	SocketState m_current_state = SocketState::Uninitialized;
	SOCKET m_socket = 0;
	int m_state = 0;
	const std::string m_delimiter = "-+";
	std::string m_host;
	std::string m_port;
	Logger m_logger{g_root_logger, "SignalingSocket"};
};

inline SignalingSocket g_signaling_socket;