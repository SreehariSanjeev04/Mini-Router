#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>
#include <map>
#include <iomanip>
#include <iostream>
#include <optional>
#include "commons/Constants.h"

namespace ARP  {
    
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

    class Cache {
        private:
            std::map<std::array<uint8_t, 4>, std::array<uint8_t, 6>> cache;
        public:
            Cache() = default;

            void put(const std::array<uint8_t, 4>& ip, const std::array<uint8_t, 6>& mac);
            std::optional<std::array<uint8_t, 6>> get(const std::array<uint8_t, 4>& ip) const;
            bool contains(const std::array<uint8_t, 4>& ip) const;
            void print() const;
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
    bool handleARPPacket(const uint8_t* data, size_t length);
    
}