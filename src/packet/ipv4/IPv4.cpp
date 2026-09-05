#include "IPv4.h"
#include "IPv4Packet.h"
#include "commons/Constants.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>

namespace IPv4
{
    /**
     * Parses an IPv4 packet from a byte buffer.
     * The wire-format IPv4 header is copied verbatim into packet.header (fields
     * stay in network byte order); the payload is pointed into the source buffer.
     * @param data Pointer to the byte buffer containing the IPv4 packet.
     * @param length Length of the byte buffer.
     * @param packet Reference to an IPv4Packet structure where the parsed data will be stored
     * @return True if parsing was successful, false otherwise.
     */
    bool parse(
        const uint8_t *data,
        size_t length,
        IPv4Packet &packet)
    {
        if (data == nullptr || length < sizeof(IPv4Header))
        {
            return false;
        }

        std::memcpy(&packet.header, data, sizeof(IPv4Header));

        uint8_t ihl = packet.header.versionAndIHL & 0x0F;
        if ((packet.header.versionAndIHL >> 4) != Net::IPv4::VERSION || ihl < Net::IPv4::MINIMUM_HEADER_SIZE)
        {
            return false;
        }

        size_t headerLengthBytes = static_cast<size_t>(ihl) * 4;
        if (length < headerLengthBytes)
        {
            return false;
        }

        // Validate total length against header size and total buffer length.
        uint16_t totalLength = ntohs(packet.header.totalLength);
        if (totalLength < headerLengthBytes || length < totalLength)
        {
            return false;
        }

        packet.payload = data + headerLengthBytes;
        packet.payloadLength = totalLength - headerLengthBytes;
        return true;
    }

    /**
     * Serializes an IPv4Packet structure into a byte buffer.
     * The header is written verbatim (it already carries network byte order).
     * @param packet The IPv4Packet structure to serialize.
     * @param buffer The byte buffer to write the serialized data into.
     * @param bufferSize The size of the byte buffer.
     * @return True if serialization was successful, false otherwise.
     */
    bool serialize(
        const struct IPv4Packet &packet,
        uint8_t *buffer,
        size_t bufferSize)
    {
        if (buffer == nullptr || bufferSize < sizeof(IPv4Header) + packet.payloadLength)
        {
            return false;
        }

        std::memcpy(buffer, &packet.header, sizeof(IPv4Header));

        if (packet.payload != nullptr && packet.payloadLength > 0)
        {
            std::memcpy(buffer + sizeof(IPv4Header), packet.payload, packet.payloadLength);
        }

        return true;
    }

    /**
     * Calculates the checksum for the IPv4 header.
     * @param data Pointer to the IPv4 header data.
     * @param headerLength Length of the IPv4 header in bytes.
     * @return The calculated checksum value.
     */
    uint16_t calculateChecksum(
        const uint8_t *data,
        size_t headerLength)
    {
        uint32_t sum = 0;
        for (size_t i = 0; i < headerLength; i += 2)
        {
            uint16_t word = data[i] << 8;
            if (i + 1 < headerLength)
            {
                word |= data[i + 1];
            }
            sum += word;
        }

        while (sum >> 16)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        return static_cast<uint16_t>(~sum);
    }

    /**
     * Verifies the checksum of the IPv4 header.
     * @param data Pointer to the IPv4 header data.
     * @param headerLength Length of the IPv4 header in bytes.
     * @return True if the checksum is valid, false otherwise.
     */
    bool verifyChecksum(
        const uint8_t *data,
        size_t headerLength)
    {
        if (data == nullptr || headerLength < 20)
        {
            return false;
        }

        uint16_t calculatedChecksum = calculateChecksum(data, headerLength);
        if (calculatedChecksum == 0x0000)
        {
            return true;
        }
        return false;
    }

    void updateChecksum(
        uint8_t *header,
        size_t headerLength)
    {
        if (header == nullptr || headerLength < 20)
        {
            return;
        }

        // Zero the checksum field (bytes 10-11) before recomputing.
        header[10] = 0;
        header[11] = 0;

        uint16_t check = calculateChecksum(header, headerLength);
        // Store in network byte order (big-endian) to match verification.
        header[10] = static_cast<uint8_t>(check >> 8);
        header[11] = static_cast<uint8_t>(check & 0xFF);
    }

    // not required, just for debugging purposes
    void printPacket(
        const struct IPv4Packet &packet
    ) {
        uint16_t flagsAndOffset = ntohs(packet.header.flagsAndFragmentOffset);
        std::array<uint8_t, 4> src = sourceAddress(packet);
        std::array<uint8_t, 4> dst = destinationAddress(packet);

        std::cout << "IPv4 Packet:" << std::endl;
        std::cout << "Version: " << (int)(packet.header.versionAndIHL >> 4) << std::endl;
        std::cout << "Header Length: " << (int)(packet.header.versionAndIHL & 0x0F) << " (words)" << std::endl;
        std::cout << "Type of Service: " << (int)packet.header.tos << std::endl;
        std::cout << "Total Length: " << ntohs(packet.header.totalLength) << std::endl;
        std::cout << "Identification: " << ntohs(packet.header.identification) << std::endl;
        std::cout << "Flags: " << (flagsAndOffset >> 13) << std::endl;
        std::cout << "Fragment Offset: " << (flagsAndOffset & 0x1FFF) << std::endl;
        std::cout << "Time to Live: " << (int)packet.header.ttl << std::endl;
        std::cout << "Protocol: " << (int)packet.header.protocol << std::endl;
        std::cout << "Header Checksum: 0x" << std::hex << ntohs(packet.header.checksum) << std::dec << std::endl;
        std::cout << "Source Address: " << ipToString(src) << std::endl;
        std::cout << "Destination Address: " << ipToString(dst) << std::endl;
    }

    uint8_t headerLengthBytes(
        const struct IPv4Packet &packet)
    {
        return static_cast<uint8_t>((packet.header.versionAndIHL & 0x0F) * 4);
    }

    uint16_t totalLength(
        const struct IPv4Packet &packet)
    {
        return static_cast<uint16_t>(ntohs(packet.header.totalLength));
    }

    std::array<uint8_t, 4> sourceAddress(
        const struct IPv4Packet &packet)
    {
        uint32_t raw = ntohl(packet.header.source);
        return {
            static_cast<uint8_t>(raw >> 24),
            static_cast<uint8_t>(raw >> 16),
            static_cast<uint8_t>(raw >> 8),
            static_cast<uint8_t>(raw)
        };
    }

    std::array<uint8_t, 4> destinationAddress(
        const struct IPv4Packet &packet)
    {
        uint32_t raw = ntohl(packet.header.destination);
        return {
            static_cast<uint8_t>(raw >> 24),
            static_cast<uint8_t>(raw >> 16),
            static_cast<uint8_t>(raw >> 8),
            static_cast<uint8_t>(raw)
        };
    }

    std::string ipToString(
        const std::array<uint8_t, 4> &address)
    {
        return std::to_string(address[0]) + "." +
               std::to_string(address[1]) + "." +
               std::to_string(address[2]) + "." +
               std::to_string(address[3]);
    }
}
