#include "Ethernet.h"

namespace Ethernet
{
    bool parseEthernetHeader(const uint8_t *data, size_t length, EthernetHeader &header)
    {
        if (length < sizeof(EthernetHeader))
        {
            return false;
        }

        uint16_t rawEtherType;
        int offset = 0;

        std::memcpy(&header.destination, data + offset, sizeof(MacAddress));
        offset += sizeof(MacAddress);
        std::memcpy(&header.source, data + offset, sizeof(MacAddress));
        offset += sizeof(MacAddress);
        std::memcpy(&rawEtherType, data + offset, sizeof(rawEtherType));
        offset += sizeof(rawEtherType);
        header.ethertype = ntohs(rawEtherType);
        return true;
    }

    std::string getMacAddressString(const MacAddress &mac)
    {
        char buffer[18];
        std::snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
                      mac.bytes[0], mac.bytes[1], mac.bytes[2],
                      mac.bytes[3], mac.bytes[4], mac.bytes[5]);
        return std::string(buffer);
    }
}