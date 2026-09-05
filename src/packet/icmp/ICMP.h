#pragma once
#pragma pack(push, 1)

#include <cstddef>
#include <cstdint>

namespace ICMP {

    namespace Type {
        constexpr uint8_t EchoReply = 0;
        constexpr uint8_t EchoRequest = 8;
    }

    namespace Code {
        constexpr uint8_t NoCode = 0;
    }

    /**
     * ICMP echo message header (echo request/reply). The payload follows the
     * header on the wire and is included in the checksum.
     */
    struct EchoHeader {
        uint8_t type;
        uint8_t code;
        uint16_t checksum;
        uint16_t identifier;
        uint16_t sequence;
    };

    struct ICMPErrorHeader {
        uint8_t type;
        uint8_t code;
        uint8_t checksum;
        uint32_t data;
    };

    /**
     * Parses an ICMP echo request from a buffer.
     * @param data Pointer to the ICMP message (the IPv4 payload).
     * @param length Length of the ICMP message.
     * @param header Reference to the EchoHeader to fill.
     * @param payload On success, set to point just past the echo header.
     * @param payloadLength On success, set to the number of payload bytes.
     * @return True if this is a well-formed echo request with a valid checksum.
     */
    bool parseEchoRequest(
        const uint8_t* data,
        size_t length,
        EchoHeader& header,
        const uint8_t*& payload,
        size_t& payloadLength);

    /**
     * Builds an ICMP echo reply for the given request.
     * @param requestHeader The header of the received echo request.
     * @param payload The echo payload to copy into the reply.
     * @param payloadLength Length of the echo payload.
     * @param buffer Buffer to write the serialized reply into.
     * @param bufferSize Size of the buffer.
     * @return True if the reply was serialized successfully.
     */
    bool createEchoReply(
        const EchoHeader& requestHeader,
        const uint8_t* payload,
        size_t payloadLength,
        uint8_t* buffer,
        size_t bufferSize);

    /**
     * Calculates the ICMP checksum (RFC 1071) over header and payload.
     * @param data Pointer to the ICMP message.
     * @param length Length of the ICMP message in bytes.
     * @return The computed checksum.
     */
    uint16_t calculateChecksum(
        const uint8_t* data,
        size_t length);

    bool createICMPErrorReply(
    const IPv4Packet& packet,
    size_t IPv4HeaderLength,
    const uint8_t& icmp_code,
    const uint8_t& icmp_type,
    uint8_t* buffer);

} // namespace ICMP

#pragma pack(pop)