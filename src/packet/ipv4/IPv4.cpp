#include "IPv4.h"
#include "IPv4Packet.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>

namespace IPv4
{
    /**
     * Parses an IPv4 packet from a byte buffer.
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
        if (data == nullptr || length < 20)
        {
            return false;
        }

        uint8_t versionAndHeaderLength;
        uint16_t totalLength;
        uint16_t identification;
        uint16_t flagsAndFragmentOffset;
        uint16_t headerChecksum;

        int offset = 0;
        std::memcpy(&versionAndHeaderLength, data + offset, sizeof(versionAndHeaderLength));
        packet.version = versionAndHeaderLength >> 4;
        packet.headerLength = versionAndHeaderLength & 0x0F;
        offset += sizeof(versionAndHeaderLength);

        if (packet.version != 4 || packet.headerLength < 5)
        {
            return false;
        }

        int headerLengthBytes = packet.headerLength * 4;

        if (length < static_cast<size_t>(headerLengthBytes))
        {
            return false;
        }

        std::memcpy(&packet.tos, data + offset, sizeof(packet.tos));
        offset += sizeof(packet.tos);

        std::memcpy(&totalLength, data + offset, sizeof(totalLength));
        packet.totalLength = ntohs(totalLength);
        offset += sizeof(totalLength);

        // Validate total length against header size and total buffer length
        if (packet.totalLength < headerLengthBytes || length < packet.totalLength)
        {
            return false;
        }

        std::memcpy(&identification, data + offset, sizeof(identification));
        packet.identification = ntohs(identification);
        offset += sizeof(identification);

        std::memcpy(&flagsAndFragmentOffset, data + offset, sizeof(flagsAndFragmentOffset));
        uint16_t hostFlagsAndOffset = ntohs(flagsAndFragmentOffset);
        packet.flags = hostFlagsAndOffset >> 13;
        packet.fragmentOffset = hostFlagsAndOffset & 0x1FFF;
        offset += sizeof(flagsAndFragmentOffset);

        std::memcpy(&packet.ttl, data + offset, sizeof(packet.ttl));
        offset += sizeof(packet.ttl);

        std::memcpy(&packet.protocol, data + offset, sizeof(packet.protocol));
        offset += sizeof(packet.protocol);

        std::memcpy(&headerChecksum, data + offset, sizeof(headerChecksum));
        packet.headerChecksum = ntohs(headerChecksum);
        offset += sizeof(headerChecksum);

        std::memcpy(packet.sourceAddress.bytes.data(), data + offset, sizeof(packet.sourceAddress.bytes));
        offset += sizeof(packet.sourceAddress.bytes);

        std::memcpy(packet.destinationAddress.bytes.data(), data + offset, sizeof(packet.destinationAddress.bytes));
        offset += sizeof(packet.destinationAddress.bytes);

        packet.payload = data + headerLengthBytes;
        packet.payloadLength = packet.totalLength - headerLengthBytes;
        return true;
    }

    /**
     * Serializes an IPv4Packet structure into a byte buffer.
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
        if (buffer == nullptr || bufferSize < packet.totalLength)
        {
            return false;
        }

        int offset = 0;
        uint8_t versionAndHeaderLength = (packet.version << 4) | (packet.headerLength & 0x0F);
        std::memcpy(buffer + offset, &versionAndHeaderLength, sizeof(versionAndHeaderLength));
        offset += sizeof(versionAndHeaderLength);

        std::memcpy(buffer + offset, &packet.tos, sizeof(packet.tos));
        offset += sizeof(packet.tos);

        uint16_t totalLengthNetworkOrder = htons(packet.totalLength);
        std::memcpy(buffer + offset, &totalLengthNetworkOrder, sizeof(totalLengthNetworkOrder));
        offset += sizeof(totalLengthNetworkOrder);

        uint16_t identificationNetworkOrder = htons(packet.identification);
        std::memcpy(buffer + offset, &identificationNetworkOrder, sizeof(identificationNetworkOrder));
        offset += sizeof(identificationNetworkOrder);

        uint16_t flagsAndFragmentOffsetNetworkOrder = htons((packet.flags << 13) | (packet.fragmentOffset & 0x1FFF));
        std::memcpy(buffer + offset, &flagsAndFragmentOffsetNetworkOrder, sizeof(flagsAndFragmentOffsetNetworkOrder));
        offset += sizeof(flagsAndFragmentOffsetNetworkOrder);

        std::memcpy(buffer + offset, &packet.ttl, sizeof(packet.ttl));
        offset += sizeof(packet.ttl);

        std::memcpy(buffer + offset, &packet.protocol, sizeof(packet.protocol));
        offset += sizeof(packet.protocol);

        uint16_t headerChecksumNetworkOrder = htons(packet.headerChecksum);
        std::memcpy(buffer + offset, &headerChecksumNetworkOrder, sizeof(headerChecksumNetworkOrder));
        offset += sizeof(headerChecksumNetworkOrder);

        std::memcpy(buffer + offset, packet.sourceAddress.bytes.data(), sizeof(packet.sourceAddress.bytes));
        offset += sizeof(packet.sourceAddress.bytes);

        std::memcpy(buffer + offset, packet.destinationAddress.bytes.data(), sizeof(packet.destinationAddress.bytes));
        offset += sizeof(packet.destinationAddress.bytes);

        if (packet.payload != nullptr && packet.payloadLength > 0)
        {
            std::memcpy(buffer + offset, packet.payload, packet.payloadLength);
            offset += packet.payloadLength;
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
        std::cout << "IPv4 Packet:" << std::endl;
        std::cout << "Version: " << (int)packet.version << std::endl;
        std::cout << "Header Length: " << (int)packet.headerLength << " (words)" << std::endl;
        std::cout << "Type of Service: " << (int)packet.tos << std::endl;
        std::cout << "Total Length: " << packet.totalLength << std::endl;
        std::cout << "Identification: " << packet.identification << std::endl;
        std::cout << "Flags: " << (int)packet.flags << std::endl;
        std::cout << "Fragment Offset: " << packet.fragmentOffset << std::endl;
        std::cout << "Time to Live: " << (int)packet.ttl << std::endl;
        std::cout << "Protocol: " << (int)packet.protocol << std::endl;
        std::cout << "Header Checksum: 0x" << std::hex << packet.headerChecksum << std::dec << std::endl;
        std::cout << "Source Address: "
                  << (int)packet.sourceAddress.bytes[0] << "."
                  << (int)packet.sourceAddress.bytes[1] << "."
                  << (int)packet.sourceAddress.bytes[2] << "."
                  << (int)packet.sourceAddress.bytes[3] << std::endl;
        std::cout << "Destination Address: "
                  << (int)packet.destinationAddress.bytes[0] << "."
                  << (int)packet.destinationAddress.bytes[1] << "."
                  << (int)packet.destinationAddress.bytes[2] << "."
                  << (int)packet.destinationAddress.bytes[3] << std::endl;
    }
}
