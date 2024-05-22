
#include "CrownLink.h"

#define BUFFER_SIZE 4096
constexpr auto ADDRESS_SIZE = 16;

namespace CLNK {

void JuiceP2P::initialize() {
	g_root_logger.info("Initializing, version {}", CL_VERSION);
	g_signaling_socket.initialize();
	m_signaling_thread = std::jthread{&JuiceP2P::receive_signaling, this};
}

void JuiceP2P::destroy() {   
	g_root_logger.info("Shutting down");
	m_is_running = false;
	// TODO: cleanup properly so we don't get an error on close
}

void JuiceP2P::requestAds() {
	g_root_logger.debug("Requesting lobbies");
	g_signaling_socket.request_advertisers();
	for (const auto& advertiser : m_known_advertisers) {
		if (g_juice_manager.peer_status(advertiser) == JUICE_STATE_CONNECTED || g_juice_manager.peer_status(advertiser) == JUICE_STATE_COMPLETED) {
			g_root_logger.trace("Requesting game state from {}", base64::to_base64(std::string((char*)advertiser.address, sizeof(SNETADDR))));
			g_signaling_socket.send_packet(advertiser, SignalMessageType::SolicitAds);
		}
	}
}

void JuiceP2P::sendAsyn(const SNETADDR& peer_ID, Util::MemoryFrame packet){
	g_juice_manager.send_p2p(std::string((char*)peer_ID.address, sizeof(SNETADDR)), packet);
}

void JuiceP2P::receive_signaling(){
	std::vector<SignalPacket> incoming_packets;
	while (m_is_running) {
		g_signaling_socket.receive_packets(incoming_packets);
		// g_logger.trace("received incoming signaling");
		for (const auto& packet : incoming_packets) {
			switch (packet.message_type) {
				case SignalMessageType::StartAdvertising: {
					g_root_logger.debug("server confirmed lobby open");
				} break;
				case SignalMessageType::StopAdvertising: {
					g_root_logger.debug("server confirmed lobby closed");
				} break;
				case SignalMessageType::RequestAdvertisers: {
					// list of advertisers returned
					// split into individual addresses & create juice peers
					update_known_advertisers(packet.data);
				} break;
				case SignalMessageType::SolicitAds: {
					if (m_is_advertising) {
						g_root_logger.debug("received solicitation from {}, replying with our lobby info", packet.peer_id.b64());
						std::string send_buffer;
						send_buffer.append((char*)m_ad_data.begin(), m_ad_data.size());
						g_signaling_socket.send_packet(packet.peer_id, SignalMessageType::GameAd,
							base64::to_base64(send_buffer));
					}
				} break;
				case SignalMessageType::GameAd: {
					// -------------- PACKET: GAME STATS -------------------------------
					// Give the ad to storm
					g_root_logger.debug("received lobby info from {}", packet.peer_id.b64());
					auto decoded_data = base64::from_base64(packet.data);
					AdFile ad{};
					memcpy_s(&ad, sizeof(ad), decoded_data.c_str(), decoded_data.size());
					SNP::passAdvertisement(packet.peer_id, Util::MemoryFrame::from(ad));

					g_root_logger.debug("Game Info Received:\n"
						"  dwIndex: {}\n"
						"  dwGameState: {}\n"
						"  saHost: {}\n"
						"  dwTimer: {}\n"
						"  szGameName[128]: {}\n"
						"  szGameStatString[128]: {}\n"
						"  dwExtraBytes: {}\n"
						"  dwProduct: {}\n"
						"  dwVersion: {}\n",
						ad.game_info.dwIndex,
						ad.game_info.dwGameState,
						ad.game_info.saHost.b64(),
						ad.game_info.dwTimer,
						ad.game_info.szGameName,
						ad.game_info.szGameStatString,
						ad.game_info.dwExtraBytes,
						ad.game_info.dwProduct,
						ad.game_info.dwVersion
					);
				} break;
				case SignalMessageType::JuiceLocalDescription:
				case SignalMessageType::JuciceCandidate:
				case SignalMessageType::JuiceDone: {
					g_juice_manager.signal_handler(packet);
				} break;
			}
		}
	}
}

void JuiceP2P::update_known_advertisers(const std::string& data) {
	m_known_advertisers.clear();
	// SNETADDR in base64 encoding is always 24 characters
	g_root_logger.trace("[update_known_advertisers] data received: {}", data);
	for (size_t i = 0; (i + 1) * 24 < data.size() + 1; i++) {
		try {
			auto peer_str = base64::from_base64(data.substr(i*24, 24));
			g_root_logger.debug("[update_known_advertisers] potential lobby owner received: {}", data.substr(i*24, 24));
			m_known_advertisers.push_back(SNETADDR(peer_str));
			g_juice_manager.create_if_not_exist(peer_str);
		}
		catch (const std::exception &exc) {
			g_root_logger.error("[update_known_advertisers] processing: {} error: {}",data.substr(i,24), exc.what());

		}
	}
}

void JuiceP2P::startAdvertising(Util::MemoryFrame ad) {
	m_ad_data = ad;
	m_is_advertising = true;
	g_signaling_socket.start_advertising();
	g_root_logger.info("started advertising lobby");
}

void JuiceP2P::stopAdvertising() {
	m_is_advertising = false;
	g_signaling_socket.stop_advertising();
	g_root_logger.info("stopped advertising lobby");
}
//------------------------------------------------------------------------------------------------

};
