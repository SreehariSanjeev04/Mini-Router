#pragma once
#pragma pack(push, 1)

#include <array>
#include <cstddef>
#include <cstdint>

namespace ARP {

struct Message {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_size;
    uint8_t protocol_size;
    uint16_t opcode;
    std::array<uint8_t, 6> sender_mac;
    std::array<uint8_t, 4> sender_ip;
    std::array<uint8_t, 6> target_mac;
    std::array<uint8_t, 4> target_ip;
};

bool parse(const uint8_t* data, size_t length, Message& message);
bool isRequest(const Message& message);
bool isReply(const Message& message);
bool isValid(const Message& message);

Message createReply(
    const Message& message,
    const std::array<uint8_t, 6>& senderMac,
    const std::array<uint8_t, 4>& sendIP
);

bool serialize(const Message& message, uint8_t* buffer, size_t bufferSize);

} // namespace ARP

#pragma pack(pop)