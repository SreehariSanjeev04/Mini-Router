#include "Ethernet.h"
#include "arp/ARP.h"
#include "commons/Constants.h"
#include <iostream>

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

    /**
     * Serializes an Ethernet header into the given buffer.
     * @param header The Ethernet header to serialize.
     * @param buffer Pointer to the buffer where the serialized header will be stored.
     * @param bufferSize Size of the buffer.
     * @return True if serialization was successful, false otherwise.
     */
    bool serialize(const EthernetHeader& header, uint8_t* buffer, size_t bufferSize)
    {
        if (bufferSize < sizeof(EthernetHeader))
        {
            return false;
        }

        size_t offset = 0;
        std::memcpy(buffer + offset, &header.destination, sizeof(MacAddress));
        offset += sizeof(MacAddress);
        std::memcpy(buffer + offset, &header.source, sizeof(MacAddress));
        offset += sizeof(MacAddress);
        uint16_t rawEtherType = htons(header.ethertype);
        std::memcpy(buffer + offset, &rawEtherType, sizeof(rawEtherType));
        return true;
    }

    /**
     * Prepends the Ethernet header onto a payload held in the given frame buffer.
     * @param buffer Frame buffer containing the payload; must have room for sizeof(EthernetHeader) additional bytes.
     * @param bufferSize Size of the payload in bytes.
     * @param header The Ethernet header to add.
     * @return True if the header was added, false otherwise.
     */
    bool addEthernetHeader(uint8_t* buffer, size_t bufferSize, const EthernetHeader& header)
    {
        std::memmove(buffer + sizeof(EthernetHeader), buffer, bufferSize);
        return serialize(header, buffer, bufferSize + sizeof(EthernetHeader));
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