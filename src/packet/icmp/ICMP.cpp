#include "ICMP.h"
#include "commons/Constants.h"
#include "packet/ipv4/IPv4Packet.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>

namespace ICMP
{

  /**
   * Parses an ICMP echo request, verifying its checksum.
   * @param data Pointer to the ICMP message.
   * @param length Length of the ICMP message.
   * @param header Reference to fill with the parsed header.
   * @param payload Set to the start of the echo payload.
   * @param payloadLength Set to the number of payload bytes.
   * @return True on success, false for invalid/short/corrupt messages.
   */
  bool parseEchoRequest(const uint8_t *data, size_t length, EchoHeader &header,
                        const uint8_t *&payload, size_t &payloadLength)
  {
    if (data == nullptr || length < sizeof(EchoHeader))
    {
      return false;
    }

    std::memcpy(&header, data, sizeof(EchoHeader));

    if (header.type != Type::EchoRequest || header.code != Code::NoCode)
    {
      return false;
    }

    // A correct checksum yields 0 when computed over the full message,
    // including the checksum field itself.
    if (calculateChecksum(data, length) != 0)
    {
      return false;
    }
    header.identifier = ntohs(header.identifier);
    header.sequence = ntohs(header.sequence);

    payload = data + sizeof(EchoHeader);
    payloadLength = length - sizeof(EchoHeader);
    return true;
  }

bool createICMPErrorReply(
    const IPv4Packet& packet,
    size_t IPv4HeaderLength,
    const uint8_t& icmp_code,
    const uint8_t& icmp_type,
    uint8_t* buffer)
{
    if (IPv4HeaderLength < 20)
        return false;

    if (icmp_code != static_cast<uint8_t>(
            Net::ICMP::Code::DEST_UNREACHABLE) ||
        icmp_type != static_cast<uint8_t>(
            Net::ICMP::Type::DEST_UNREACHABLE))
        return false;

    ICMPErrorHeader reply{};

    reply.code = icmp_code;
    reply.type = icmp_type;
    reply.checksum = 0;
    reply.data = 0;

  
    std::memcpy(buffer, &reply, sizeof(reply));


    std::memcpy(
        buffer + sizeof(reply),
        &packet.header,
        IPv4HeaderLength
    );

    std::memcpy(
        buffer + sizeof(reply) + IPv4HeaderLength,
        packet.payload,
        8
    );

    uint16_t checkSum = htons(
        calculateChecksum(
            buffer,
            sizeof(reply) + IPv4HeaderLength + 8
        )
    );

    std::memcpy(
        buffer + offsetof(ICMPErrorHeader, checksum),
        &checkSum,
        sizeof(checkSum)
    );

    return true;
}

  /**
   * Serializes an ICMP echo reply into the given buffer.
   * The reply mirrors the request's identifier, sequence and payload, sets
   * type to EchoReply and recomputes the checksum.
   * @param requestHeader The parsed header of the incoming request.
   * @param payload The echo payload from the request.
   * @param payloadLength Length of the payload.
   * @param buffer Destination buffer.
   * @param bufferSize Size of the destination buffer.
   * @return True on success, false if the buffer is too small.
   */
  bool createEchoReply(const EchoHeader &requestHeader, const uint8_t *payload,
                       size_t payloadLength, uint8_t *buffer, size_t bufferSize)
  {
    if (buffer == nullptr || bufferSize < sizeof(EchoHeader) + payloadLength)
    {
      return false;
    }

    EchoHeader reply;
    reply.type = Type::EchoReply;
    reply.code = Code::NoCode;
    reply.checksum = 0;
    reply.identifier = htons(requestHeader.identifier);
    reply.sequence = htons(requestHeader.sequence);

    std::memcpy(buffer, &reply, sizeof(reply));
    if (payloadLength > 0)
    {
      std::memcpy(buffer + sizeof(reply), payload, payloadLength);
    }

    // Store the checksum in network byte order (big-endian) so that
    // verification re-spelling the words big-endian reproduces the value.
    uint16_t check = calculateChecksum(buffer, sizeof(reply) + payloadLength);
    buffer[offsetof(EchoHeader, checksum)] = static_cast<uint8_t>(check >> 8);
    buffer[offsetof(EchoHeader, checksum) + 1] =
        static_cast<uint8_t>(check & 0xFF);

    return true;
  }

  /**
   * Computes the ICMP checksum over the full message (header + payload).
   * @param data Pointer to the ICMP message.
   * @param length Length in bytes.
   * @return The one's-complement checksum.
   */
  uint16_t calculateChecksum(const uint8_t *data, size_t length)
  {
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < length; i += 2)
    {
      uint16_t word = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
      sum += word;
    }
    if (length % 2 != 0)
    {
      sum += static_cast<uint16_t>(data[length - 1]) << 8;
    }

    while (sum >> 16)
    {
      sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
  }

} // namespace ICMP
