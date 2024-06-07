#include "SNPModule.h"

#include "CrownLink.h"
#include <list>

namespace snp {

struct SNPContext {
	client_info game_app_info;

	std::list<AdFile> game_list;
	s32 next_game_ad_id = 1;

	AdFile hosted_game;
	AdFile status_ad;
	bool   status_ad_used = false;
};

static SNPContext g_snp_context;

void pass_advertisement(const NetAddress& host, AdFile& ad) {
	std::lock_guard lock{g_advertisement_mutex};

	AdFile* adFile = nullptr;
	for (auto& game : g_snp_context.game_list) {
		if (!memcmp(&game.game_info.saHost, &host, sizeof(NetAddress))) {
			adFile = &game;
			break;
		}
	}

	if (!adFile) {
		adFile = &g_snp_context.game_list.emplace_back();
		ad.game_info.dwIndex = ++g_snp_context.next_game_ad_id;
	} else {
		ad.game_info.dwIndex = adFile->game_info.dwIndex;
	}

	memcpy_s(adFile, sizeof(AdFile), &ad, sizeof(AdFile));

	std::string prefix;
	if (g_snp_context.game_app_info.dwVerbyte != adFile->game_info.dwVersion) {
		spdlog::info("Version byte mismatch. ours: {} theirs: {}", g_snp_context.game_app_info.dwVerbyte, adFile->game_info.dwVersion);
		prefix += "[!Ver]";
	}

	switch (g_crown_link->juice_manager().agent_state(host)) {
	case JUICE_STATE_CONNECTING:{
		prefix += "[P2P Connecting]";
	} break;
	case JUICE_STATE_FAILED: {
		prefix += "[P2P Failed]";
	} break;
	case JUICE_STATE_DISCONNECTED:{
		prefix += "[P2P Not Connected]";
	} break;
	}

	switch (g_crown_link->juice_manager().final_connection_type(host)) {
	case JuiceConnectionType::Relay: {
		prefix += "[Relayed]";
	} break;
	case JuiceConnectionType::Radmin:{
		prefix += "[Radmin]";
	} break;
	}

	if (!prefix.empty()) {
		prefix += " ";
		prefix += adFile->game_info.szGameName;
		strncpy_s(adFile->game_info.szGameName, sizeof(adFile->game_info.szGameName), prefix.c_str(), sizeof(adFile->game_info.szGameName));
	}

	adFile->game_info.dwTimer = GetTickCount();
	adFile->game_info.saHost = *(SNETADDR*)&host;
	adFile->game_info.pExtra = adFile->extra_bytes;
}

void remove_advertisement(const NetAddress& host) {}

void pass_packet(GamePacket& packet) {}

BOOL __stdcall spi_initialize(client_info* client_info, user_info* user_info, battle_info* callbacks, module_info* module_data, HANDLE event) {
	g_snp_context.game_app_info = *client_info;
	g_receive_event = event;
	auto log_filename = (g_starcraft_dir / "crownlink_logs" / "CrownLink.txt").generic_wstring();
	auto g_logger = spdlog::daily_logger_mt<spdlog::async_factory>("cl", log_filename, 2, 30);
	g_logger->set_level(spdlog::level::info);
	g_logger->flush_on(spdlog::level::err);
	spdlog::set_default_logger(g_logger);
	spdlog::enable_backtrace(32);
	set_status_ad("Crownlink Initializing");
	spdlog::info("Crownlink Initializing");
	try {
		g_crown_link = std::make_unique<CrownLink>();
	} catch (std::exception& e) {
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}

	return true;
}

BOOL __stdcall spi_destroy() {
	try {
		g_crown_link.reset();
	} catch (std::exception& e) {
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}
	spdlog::shutdown();

	return true;
}

BOOL __stdcall spi_lock_game_list(int, int, game** out_game_list) {
	std::lock_guard lock{g_advertisement_mutex};

	std::erase_if(g_snp_context.game_list, [now = GetTickCount()](const auto& current_ad) { return now > current_ad.game_info.dwTimer + 2000; });

	AdFile* last_ad = nullptr;
	for (auto& game : g_snp_context.game_list) {
		game.game_info.pExtra = game.extra_bytes;
		if (last_ad) {
			last_ad->game_info.pNext = &game.game_info;
		}

		last_ad = &game;
	}
	if (last_ad) {
		last_ad->game_info.pNext = nullptr;
	}

	if (last_ad && g_snp_context.status_ad_used) {
		last_ad->game_info.pNext = &g_snp_context.status_ad.game_info;
		g_snp_context.status_ad.game_info.dwIndex = last_ad->game_info.dwIndex + 1;
	}

	try {
		*out_game_list = nullptr;
		if (!g_snp_context.game_list.empty()) {
			
			*out_game_list = &g_snp_context.game_list.begin()->game_info;
		}
		else if(g_snp_context.status_ad_used) {
			*out_game_list = &g_snp_context.status_ad.game_info;
		}
	} catch (std::exception& e) {
		spdlog::dump_backtrace();
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}
	return true;
}

BOOL __stdcall spi_unlock_game_list(game* game_list, DWORD*) {
	try {
		g_crown_link->request_advertisements();
	} catch (std::exception& e) {
		spdlog::dump_backtrace();
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}

	return true;
}

static void create_ad(AdFile& ad_file, const char* game_name, const char* game_stat_string, DWORD game_state, void* user_data, DWORD user_data_size) {
	memset(&ad_file, 0, sizeof(ad_file));

	auto& game_info = ad_file.game_info;
	strcpy_s(game_info.szGameName, sizeof(game_info.szGameName), game_name);
	strcpy_s(game_info.szGameStatString, sizeof(game_info.szGameStatString), game_stat_string);
	game_info.dwGameState = game_state;
	game_info.dwProduct = g_snp_context.game_app_info.dwProduct;
	game_info.dwVersion = g_snp_context.game_app_info.dwVerbyte;
	game_info.dwUnk_1C = 0x0050;
	game_info.dwUnk_24 = 0x00a7;

	memcpy(ad_file.extra_bytes, user_data, user_data_size);
	game_info.dwExtraBytes = user_data_size;
	game_info.pExtra = ad_file.extra_bytes;
}

void set_status_ad(const std::string& status) {
	std::lock_guard lock{g_advertisement_mutex};
	auto statstr = std::string{",33,,3,,1e,,1,cb2edaab,5,,Server\rStatus\r"};
	char user_data[32] = {
		12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	create_ad(g_snp_context.status_ad, status.c_str(), statstr.c_str(), 0, &user_data, 32);
	g_snp_context.status_ad.game_info.pNext = nullptr;
	g_snp_context.status_ad_used = true;
}

void clear_status_ad() {
	g_snp_context.status_ad_used = false;
}

BOOL __stdcall spi_start_advertising_ladder_game(char* game_name, char* game_password, char* game_stat_string, DWORD game_state, DWORD elapsed_time, DWORD game_type, int, int, void* user_data, DWORD user_data_size) {
	std::lock_guard lock{g_advertisement_mutex};
	create_ad(g_snp_context.hosted_game, game_name, game_stat_string, game_state, user_data, user_data_size);
	g_crown_link->start_advertising(g_snp_context.hosted_game);
	return true;
}

BOOL __stdcall spi_stop_advertising_game() {
	std::lock_guard lock{g_advertisement_mutex};
	g_crown_link->stop_advertising();
	return true;
}

BOOL __stdcall spi_get_game_info(DWORD index, char* game_name, int, game* out_game) {
	std::lock_guard lock{g_advertisement_mutex};

	for (auto& game : g_snp_context.game_list) {
		if (game.game_info.dwIndex == index) {
			*out_game = game.game_info;
			return true;
		}
	}

	SErrSetLastError(STORM_ERROR_GAME_NOT_FOUND);
	return false;
}

BOOL __stdcall spi_send(DWORD address_count, NetAddress** out_address_list, char* data, DWORD size) {
	if (!address_count) {
		return true;
	}

	if (address_count > 1) {
		spdlog::info("multicast attempted");
	}

	try {
		NetAddress peer = *(out_address_list[0]);
		spdlog::trace("spiSend: {}", std::string{data, size});

		g_crown_link->send(peer, data, size);
	} catch (std::exception& e) {
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}
	return true;
}

BOOL __stdcall spi_receive(NetAddress** peer, char** out_data, DWORD* out_size) {
	*peer = nullptr;
	*out_data = nullptr;
	*out_size = 0;

	GamePacket* loan = new GamePacket{};
	try {
		while (true) {
			/*if (!g_crown_link->receive_queue().try_pop_dont_wait(*loan)) {
				SErrSetLastError(STORM_ERROR_NO_MESSAGES_WAITING);
				return false;
			}*/
			if (!g_crown_link->receive_queue().try_dequeue(*loan)) {
				SErrSetLastError(STORM_ERROR_NO_MESSAGES_WAITING);
				return false;
			}
			std::string debug_string{loan->data, loan->size};
			spdlog::trace("spiReceive: {} :: {}", loan->timestamp, debug_string);

			if (GetTickCount() > loan->timestamp + 10000) {
				continue;
			}

			*peer = &loan->sender;
			*out_data = loan->data;
			*out_size = loan->size;

			break;
		}
	} catch (std::exception& e) {
		delete loan;
		spdlog::dump_backtrace();
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}
	return true;
}

BOOL __stdcall spi_free(NetAddress* loan, char* data, DWORD size) {
	if (loan) {
		delete loan;
	}
	return true;
}

BOOL __stdcall spi_compare_net_addresses(NetAddress* address1, NetAddress* address2, DWORD* out_result) {
	if (out_result) {
		*out_result = 0;
	}
	if (!address1 || !address2 || !out_result) {
		SErrSetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	*out_result = (memcmp(address1, address2, sizeof(NetAddress)) == 0);
	return true;
}

BOOL __stdcall spi_lock_device_list(DWORD* out_device_list) {
	*out_device_list = 0;
	return true;
}

BOOL __stdcall spi_unlock_device_list(void*) {
	return true;
}

BOOL __stdcall spi_free_external_message(NetAddress* address, char* data, DWORD size) {
	return false;
}

BOOL __stdcall spi_get_performance_data(DWORD type, DWORD* out_result, int, int) {
	return false;
}

BOOL __stdcall spi_initialize_device(int, void*, void*, DWORD*, void*) {
	return false;
}

BOOL __stdcall spi_receive_external_message(NetAddress** out_address, char** out_data, DWORD* out_size) {
	if (out_address) {
		*out_address = nullptr;
	}
	if (out_data) {
		*out_data = nullptr;
	}
	if (out_size) {
		*out_size = 0;
	}
	SErrSetLastError(STORM_ERROR_NO_MESSAGES_WAITING);
	return false;
}

BOOL __stdcall spi_select_game(int, client_info* client_info, user_info* user_info, battle_info* callbacks, module_info* module_info, int) {
	// Looks like an old function and doesn't seem like it's used anymore
	// UDPN's function Creates an IPX game select dialog window
	return false;
}

BOOL __stdcall spi_send_external_message(int, int, int, int, int) {
	return false;
}

BOOL __stdcall spi_league_get_name(char* data, DWORD size) {
	return true;
}

snp::NetFunctions g_spi_functions = {
	  sizeof(snp::NetFunctions),
/*n*/ &snp::spi_compare_net_addresses,
	  &snp::spi_destroy,
	  &snp::spi_free,
/*e*/ &snp::spi_free_external_message,
      &snp::spi_get_game_info,
/*n*/ &snp::spi_get_performance_data,
      &snp::spi_initialize,
/*e*/ &snp::spi_initialize_device,
/*e*/ &snp::spi_lock_device_list,
      &snp::spi_lock_game_list,
      &snp::spi_receive,
/*n*/ &snp::spi_receive_external_message,
/*e*/ &snp::spi_select_game,
      &snp::spi_send,
/*e*/ &snp::spi_send_external_message,
/*n*/ &snp::spi_start_advertising_ladder_game,
/*n*/ &snp::spi_stop_advertising_game,
/*e*/ &snp::spi_unlock_device_list,
      &snp::spi_unlock_game_list,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
/*n*/ &snp::spi_league_get_name
};

}
