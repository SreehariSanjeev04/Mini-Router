#pragma once

#include <cstdint>

namespace Net {

    namespace Ethernet {
        enum class Type : uint16_t {
            IPv4 = 0x0800,
            ARP  = 0x0806,
            IPv6 = 0x86DD,
            VLAN = 0x8100
        };
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
    constexpr int BUFFER_SIZE = 1518; // practial maximum size of an Ethernet frame
}