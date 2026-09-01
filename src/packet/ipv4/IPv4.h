#pragma once
#include "IPv4Packet.h"

namespace IPv4 {
    
    bool parse(
        const uint8_t* data,
        size_t length,
        struct IPv4Packet& packet
    );

    uint16_t calculateChecksum(
        const uint8_t* data,
        size_t length
    );  

    bool verifyChecksum(
        const uint8_t* data,
        size_t headerLength
    );  

    /**
     * Recomputes and writes the header checksum after a field (e.g. TTL) has
     * changed, aligning with serialize() + verifyChecksum.
     * @param header Pointer to the IPv4 header bytes.
     * @param headerLength Length of the IPv4 header in bytes.
     */
    void updateChecksum(
        uint8_t* header,
        size_t headerLength
    );  

    bool serialize(
        const struct IPv4Packet& packet,
        uint8_t* buffer,
        size_t bufferSize
    );

    void printPacket(const struct IPv4Packet& packet);
}