#pragma once
#include "../shared_common.h"

struct NetAddress {
    u8 bytes[16]{};

    NetAddress() = default;
    NetAddress(u8* addr) {
        memcpy_s(&bytes, sizeof(bytes), addr, sizeof(NetAddress));
    }

    NetAddress(const std::string& id) {
        memcpy_s(&bytes, sizeof(bytes), id.c_str(), sizeof(NetAddress));
    };

    std::string b64() const {
        return base64::to_base64(std::string((char*) bytes, sizeof(NetAddress)));
    }

    bool operator==(const NetAddress&) const = default;
};

template <>
struct std::hash<NetAddress> {
    size_t operator()(const NetAddress& address) const {
        size_t hash = 0;
        for (char c : address.bytes) {
            hash <<= 1;
            hash ^= std::hash<char>{}(c);
        }
        return hash;
    }
};

struct CAPS {
    u32 size;
    u32 flags;             // 0x20000003. 0x00000001 = page locked buffers, 0x00000002 = basic interface, 0x20000000 = release mode
    u32 max_message_size;  // must be between 128 and 512
    u32 max_queue_size;
    u32 max_players;
    u32 bytes_per_second;
    u32 latency;
    u32 turns_per_second; // the turn_per_second and turns_in_transit values
    u32 turns_in_transit; // in the appended mpq file seem to be used instead
};

struct client_info {
    u32   size; // 60
    char* program_name;
    char* program_description;
    u32   program_id;
    u32   version_id;
    u32   dwUnk5;
    u32   max_players;
    u32   dwUnk7;
    u32   dwUnk8;
    u32   dwUnk9;
    u32   dwUnk10; // 0xFF
    char* cd_key;
    char* cd_owner;
    u32   is_shareware;
    u32   language_id;
};

struct user_info {
    u32   size; // 16
    char* player_name;
    char* player_description;
    u32   dwUnknown;
};

struct battle_info {
    u32   size;   // 92
    u32   dwUnkType;
    void* hFrameWnd;
    void* pfnBattleGetResource;
    void* pfnBattleGetErrorString;
    void* pfnBattleMakeCreateGameDialog;
    void* pfnBattleUpdateIcons;
    u32   dwUnk_07;
    void* pfnBattleErrorDialog;
    void* pfnBattlePlaySound;
    u32   dwUnk_10;
    void* pfnBattleGetCursorLink;
    u32   dwUnk_12;
    void* pfnUnk_13;
    u32   dwUnk_14;
    void* pfnBattleMakeProfileDialog;
    char* pszProfileStrings;
    void* pfnBattleDrawProfileInfo;
    void* pfnUnk_18;
    u32   dwUnk_19;
    void* pfnUnk_20;
    void* pfnUnk_21;
    void* pfnBattleSetLeagueName;
};

struct module_info {
    u32   size; // 20
    char* version_string;
    char* executable_file;
    char* original_archive_file;
    char* patch_archive_file;
};

struct game {
    u32     game_index;
    u32     game_state;
    u32     creation_time;
    NetAddress  host;
    u32     host_latency;
    u32     host_last_time;
    u32     category_bits;
    char    game_name[128];
    char    game_description[128];
    game*   pNext;
    void*   pExtra;
    u32     extra_bytes;
    u32     program_id;
    u32     version_id;
};

enum class CrownLinkMode {
    CLNK, // standard version
    DBCL  // double brain cells version
};

inline std::string to_string(CrownLinkMode value) {
    switch (value) {
        EnumStringCase(CrownLinkMode::CLNK);
        EnumStringCase(CrownLinkMode::DBCL);
    }
    return std::to_string((s32) value);
}

struct AdFile {
    game game_info{};
    char extra_bytes[32]{};
    CrownLinkMode crownlink_mode{};
};

enum class GamePacketType : u8 {
    System,
    Message,
    Turn,
    Types,
};

inline std::string to_string(GamePacketType value) {
    switch (value) {
        EnumStringCase(GamePacketType::System);
        EnumStringCase(GamePacketType::Message);
        EnumStringCase(GamePacketType::Turn);
        EnumStringCase(GamePacketType::Types);
    }
    return std::to_string((u8) value);
}

enum class GamePacketSubType : u8 {
    Unused,
    InitialContact,
    CircuitCheck,
    CircuiteCheckResponse,
    Ping,
    PingResponse,
    PlayerInfo,
    PlayerJoin,
    PlayerJoinAcceptStart,
    PlayerJoinAcceptDone,
    PlayerJoinReject,
    PlayerLeave,
    DropPlayer,
    NewGameOwner,
    Messages
};

inline std::string to_string(GamePacketSubType value) {
    switch (value) {
        EnumStringCase(GamePacketSubType::Unused);
        EnumStringCase(GamePacketSubType::InitialContact);
        EnumStringCase(GamePacketSubType::CircuitCheck);
        EnumStringCase(GamePacketSubType::CircuiteCheckResponse);
        EnumStringCase(GamePacketSubType::Ping);
        EnumStringCase(GamePacketSubType::PingResponse);
        EnumStringCase(GamePacketSubType::PlayerInfo);
        EnumStringCase(GamePacketSubType::PlayerJoin);
        EnumStringCase(GamePacketSubType::PlayerJoinAcceptStart);
        EnumStringCase(GamePacketSubType::PlayerJoinAcceptDone);
        EnumStringCase(GamePacketSubType::PlayerJoinReject);
        EnumStringCase(GamePacketSubType::PlayerLeave);
        EnumStringCase(GamePacketSubType::DropPlayer);
        EnumStringCase(GamePacketSubType::NewGameOwner);
        EnumStringCase(GamePacketSubType::Messages);
    }
    return std::to_string((u8) value);
}

enum class GamePacketFlags : u8 {
    Acknowledgement = 0x01,
    ResendRequest = 0x02,
    Forwareded = 0x04
};

struct GamePacketHeader { // TODO: Constructor & to_string / print method
    u32 checksum;
    u32 size;
    u32 sequence;
    u32 ack_sequence;
    GamePacketType type;
    GamePacketSubType sub_type;
    u8 player_id;
    GamePacketFlags flags;
};

struct GamePacketData { // TODO: Constructor & to_string / print method
    GamePacketHeader header;
    char payload[500]{};
};

struct GamePacket {
    NetAddress sender{};
    u32 size = 0;
    u32 timestamp = 0;
    char data[512]{}; // TODO: replace with GamePackateData

    GamePacket() = default;
    GamePacket(const NetAddress& sender_id, const char* recv_data, const size_t size)
        : sender{sender_id}, timestamp{GetTickCount()}, size{size} {
        memcpy_s(data, sizeof(data), recv_data, size);
    };
};