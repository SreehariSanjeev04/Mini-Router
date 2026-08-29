#pragma once

#include <cstdint>
#include <array>

namespace Net {

    namespace Ethernet {
        enum class Type : uint16_t {
            IPv4 = 0x0800,
            ARP  = 0x0806,
            IPv6 = 0x86DD,
            VLAN = 0x8100
        };

        constexpr int BUFFER_SIZE = 1518; // practial maximum size of an Ethernet frame
    }

    namespace ARP {
        enum class Opcode : uint16_t {
            Request = 1,
            Reply   = 2
        };

    
        constexpr uint16_t HARDWARE_ETHERNET = 1;
        constexpr uint8_t  HARDWARE_LENGTH   = 6;
        constexpr uint8_t  PROTOCOL_LENGTH   = 4;
    }

    namespace FIRST_INTERFACE {
        // 8a:68:5f:c8:07:66
        constexpr std::array<uint8_t, 6> MAC_ADDRESS = {0x8a, 0x68, 0x5f, 0xc8, 0x07, 0x66}; 
        constexpr std::array<uint8_t, 4> IP_ADDRESS = {10, 0, 1, 1}; 
    }

    namespace SECOND_INTERFACE {
        // f6:2c:3c:a7:15:81
        constexpr std::array<uint8_t, 6> MAC_ADDRESS = {0xf6, 0x2c, 0x3c, 0xa7, 0x15, 0x81}; 
        constexpr std::array<uint8_t, 4> IP_ADDRESS = {10, 0, 2, 1}; 
    }
}