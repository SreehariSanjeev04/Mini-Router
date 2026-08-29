#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <stdexcept>
#include <arpa/inet.h>

namespace Ethernet
{
    struct MacAddress
    {
        uint8_t bytes[6];
    };

    struct EthernetHeader
    {
        MacAddress destination;
        MacAddress source;
        uint16_t ethertype;
    };

    bool parseEthernetHeader(const uint8_t* data, size_t length, EthernetHeader& header);
    bool serialize(const EthernetHeader& header, uint8_t* buffer, size_t bufferSize);
    bool addEthernetHeader(uint8_t* buffer, size_t bufferSize, const EthernetHeader& header);
    std::string getMacAddressString(const MacAddress& mac);
}