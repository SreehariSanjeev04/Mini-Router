#pragma once

#include "IPv4Packet.h"

#include <array>
#include <string>

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


    /** @return The IPv4 header length in bytes (includes any options). */
    uint8_t headerLengthBytes(const struct IPv4Packet& packet);

    /** @return The total IP packet length in bytes (header + payload). */
    uint16_t totalLength(const struct IPv4Packet& packet);

    /** @return The source address as a 4-byte array. */
    std::array<uint8_t, 4> sourceAddress(const struct IPv4Packet& packet);

    /** @return The destination address as a 4-byte array. */
    std::array<uint8_t, 4> destinationAddress(const struct IPv4Packet& packet);

    /** @return A dotted-quad string for a 4-byte address. */
    std::string ipToString(const std::array<uint8_t, 4>& address);
}