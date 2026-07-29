#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <cstdint>

constexpr int PAYLOAD_SIZE = 160;
constexpr int FEC_GROUP_SIZE = 2;

enum PacketType : uint8_t
{   DATA   = 0,
    PARITY = 1
};

#pragma pack(push, 1)

struct Packet
{   uint32_t seq;
    uint16_t group;
    uint8_t type;
    uint8_t payload[PAYLOAD_SIZE];
};

#pragma pack(pop)
#endif