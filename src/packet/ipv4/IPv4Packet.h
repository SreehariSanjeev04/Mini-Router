#pragma once

#include <cstddef>
#include <cstdint>

#pragma pack(push, 1)
struct IPv4Header
{
    uint8_t versionAndIHL;
    uint8_t tos;
    uint16_t totalLength;
    uint16_t identification;
    uint16_t flagsAndFragmentOffset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t source;
    uint32_t destination;
};
#pragma pack(pop)

struct IPv4Packet
{
    IPv4Header header;
    const uint8_t* payload;
    size_t payloadLength;
};