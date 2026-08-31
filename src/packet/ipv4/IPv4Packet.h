#pragma once

#include "IPv4Address.h"

struct IPv4Packet {
    uint8_t version;
    uint8_t headerLength;

    uint8_t tos;
    uint16_t totalLength;

    uint16_t identification;

    uint8_t flags;
    uint16_t fragmentOffset;

    uint8_t ttl;
    uint8_t protocol;

    uint16_t headerChecksum;

    IPv4Address sourceAddress;
    IPv4Address destinationAddress;

    const uint8_t* payload;
    size_t payloadLength;
};