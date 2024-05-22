#include "signaling.h"

void to_json(json& out_json, const SignalPacket& packet) {
	try {
		out_json = json{
			{"peer_ID",packet.peer_id.b64()},
			{"message_type",packet.message_type},
			{"data",packet.data},
		};
	} catch (const json::exception& e) {
		g_root_logger.error("Signal packet to_json error : {}", e.what());
	}
};

void from_json(const json& json_, SignalPacket& out_packet) {
	try {
		auto tempstr = json_.at("peer_ID").get<std::string>();
		out_packet.peer_id = base64::from_base64(tempstr);
		json_.at("message_type").get_to(out_packet.message_type);
		json_.at("data").get_to(out_packet.data);
	} catch (const json::exception& ex) {
		g_root_logger.error("Signal packet from_json error: {}. JSON dump: {}", ex.what(), json_.dump());
	}
};

bool SignalingSocket::initialize() {
	m_logger.info("connecting to matchmaking server");
	m_current_state = SocketState::Connecting;

	addrinfo hints = {}; 
	addrinfo* result = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (const auto error = getaddrinfo(g_snp_config.server.c_str(), std::to_string(g_snp_config.port).c_str(), &hints, &result)) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
		m_logger.error("getaddrinfo failed with error: {}", error);
		WSACleanup();
		return false;
	}

	for (auto info = result; info; info = info->ai_next) {
		if ((m_socket = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
			m_logger.debug("client: socket failed with error: {}", std::strerror(errno));
			throw GeneralException("socket failed");
			continue;
		}

		if (connect(m_socket, info->ai_addr, info->ai_addrlen) == -1) {
			closesocket(m_socket);
			m_logger.error("client: couldn't connect to server: {}", std::strerror(errno));
			throw GeneralException("server connection failed");
			continue;
		}

		break;
	}
	if (result == NULL) {
		m_logger.error("signaling client failed to connect");
		throw GeneralException("server connection failed");
		return false;
	}

	freeaddrinfo(result);

	// server address: each byte is 11111111
	memset(&m_server, 255, sizeof(SNetAddr));

	set_blocking_mode(true);
	m_logger.info("successfully connected to matchmaking server");
	m_current_state = SocketState::Ready;
	return true;
}

void SignalingSocket::deinitialize() {
	closesocket(m_socket);
}

void SignalingSocket::send_packet(SNetAddr dest, SignalMessageType msg_type, const std::string& msg) {
	send_packet(SignalPacket(dest,msg_type,msg));
}

void SignalingSocket::send_packet(const SignalPacket& packet) {
	if (m_current_state != SocketState::Ready) {
		m_logger.error("signal send_packet attempted but provider is not ready. State: {}", as_string(m_current_state));
		return;
	}

	json json_ = packet;
	auto send_buffer = json_.dump();
	send_buffer += m_delimiter;
	m_logger.debug("Sending to server, buffer size: {}, contents: {}", send_buffer.size(), send_buffer);

	int bytes = send(m_socket, send_buffer.c_str(), send_buffer.size(), 0);
	if (bytes == -1) {
		m_logger.error("signaling send packet error: {}", std::strerror(errno));
	}
}

void SignalingSocket::split_into_packets(const std::string& s,std::vector<SignalPacket>& incoming_packets) {
	size_t pos_start = 0;
	size_t pos_end = 0;
	size_t delim_len = m_delimiter.length();
	std::string segment;

	incoming_packets.clear();
	while ((pos_end = s.find(m_delimiter, pos_start)) != std::string::npos) {
		segment = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
			
		json json_ = json::parse(segment);
		incoming_packets.push_back(json_.template get<SignalPacket>());
	}

	return;
}

void SignalingSocket::receive_packets(std::vector<SignalPacket>& incoming_packets){
	constexpr unsigned int MAX_BUF_LENGTH = 4096;
	std::vector<char> buffer(MAX_BUF_LENGTH);
	std::string receive_buffer;
	m_logger.trace("receive_packets");
	// try to receive
	auto n_bytes = recv(m_socket, &buffer[0], buffer.size(), 0);
	if (n_bytes == SOCKET_ERROR) {
		m_last_error = WSAGetLastError();
		m_logger.debug("receive error: {}", m_last_error);
		if (m_last_error == WSAEWOULDBLOCK) {
			// we're waiting for data or connection is reset
			// this is legacy of the old non-blocking code
			// we now block in a thread instead
			return;
		}
		if (m_last_error == WSAECONNRESET) {
			// connection reset by server
			// not sure if this should be a fatal or something
			m_logger.error("signaling socket receive error: connection reset by server, attempting reconnect");
			deinitialize();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			initialize();

		}
		else throw GeneralException("::recv failed");
	}
	if (n_bytes <1) {
		// 0 = connection closed by remote end
		// -1 = winsock error
		// anything more = we actually got data
		m_logger.error("server connection closed, attempting reconnect");
		deinitialize();
		std::this_thread::sleep_for(std::chrono::seconds(1));
		initialize();
	} else {
		buffer.resize(n_bytes);
		receive_buffer.append(buffer.begin(), buffer.end());
		split_into_packets(receive_buffer, incoming_packets);
		m_logger.trace("received {}", n_bytes);
		return;
	}
}

void SignalingSocket::set_blocking_mode(bool block) {
	u_long nonblock = !block;
	if (::ioctlsocket(m_socket, FIONBIO, &nonblock) == SOCKET_ERROR) {
		throw GeneralException("::ioctlsocket failed");
	}
}

void SignalingSocket::start_advertising(){
	send_packet(m_server, SignalMessageType::StartAdvertising);
}

void SignalingSocket::stop_advertising(){
	send_packet(m_server, SignalMessageType::StopAdvertising);
}

void SignalingSocket::request_advertisers(){
	send_packet(m_server, SignalMessageType::RequestAdvertisers);
}
