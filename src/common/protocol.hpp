#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <arpa/inet.h>

enum class MsgType : uint8_t {
    LOGIN = 1,
    LOGIN_OK,
    LOGIN_FAIL,
    CREATE_ROOM,
    CREATE_ROOM_OK,
    CREATE_ROOM_FAIL,
    GET_ROOM_LIST,
    ROOM_LIST,
    JOIN_ROOM,
    JOIN_ROOM_OK,
    JOIN_ROOM_FAIL,
    NEW_PLAYER_JOINED,
    PLAYER_LEFT,
    
    START_GAME,
    GAME_STARTED,
    GAME_START_FAIL,
    
    SUBMIT_ANSWERS,
    VERIFICATION_START,
    TIME_UP,
    TIME_LEFT,
    
    SEND_VOTE,
    ROUND_END,
    LEAVE_ROOM,
    HOST_LEFT,
    GAME_END 
};

struct MsgHeader {
    MsgType type;
    uint32_t len; 
} __attribute__((packed));

inline std::vector<char> createMessage(MsgType type, const std::string& data) {
    std::vector<char> buffer;
    buffer.resize(sizeof(MsgHeader) + data.size());
    
    MsgHeader* header = reinterpret_cast<MsgHeader*>(buffer.data());
    header->type = type;
    header->len = htonl(data.size());
    
    std::memcpy(buffer.data() + sizeof(MsgHeader), data.data(), data.size());
    return buffer;
}
