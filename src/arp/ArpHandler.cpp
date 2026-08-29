#include "ArpHandler.h"
#include "ARP.h"
#include "ArpCache.h"
#include "commons/Constants.h"
#include "interface/InterfaceManager.h"
#include "network/RawSocket.h"
#include "packet/ethernet/Ethernet.h"

#include <array>
#include <cstring>
#include <iostream>

/**
 * Constructs an ARP handler with the given dependencies.
 * @param cache The ARP cache used to store learned IP-to-MAC mappings.
 * @param interfaces The interface manager used to identify local interfaces.
 * @param io The raw socket used for network I/O.
 */
ArpHandler::ArpHandler(
    ArpCache& cache,
    InterfaceManager& interfaces,
    RawSocket& io)
    : cache_(cache),
      interfaces_(interfaces),
      io_(io)
{
}

/**
 * Handles an incoming ARP packet. It parses the packet, updates the ARP cache, and if the
 * request targets one of the router's interfaces, builds an Ethernet frame containing the ARP
 * reply and sends it.
 * @param data The raw packet data.
 * @param length The length of the packet data.
 * @param responseBuffer A buffer to store the generated ARP reply frame, if any.
 * @param responseBufferSize The size of the response buffer.
 * @return True if a reply was generated and sent, false otherwise.
 */
bool ArpHandler::handleARPPacket(
    const uint8_t* data,
    size_t length,
    uint8_t* responseBuffer,
    size_t responseBufferSize)
{
    if (length < sizeof(Ethernet::EthernetHeader))
    {
        return false;
    }

    ARP::Message message;
    if (!ARP::parse(data + sizeof(Ethernet::EthernetHeader), length - sizeof(Ethernet::EthernetHeader), message))
    {
        std::cerr << "Failed to parse ARP message" << std::endl;
        return false;
    }

    cache_.put(message.sender_ip, message.sender_mac);

    if (ARP::isRequest(message) && interfaces_.isRouterIp(message.target_ip))
    {
        std::array<uint8_t, 6> localMac;
        if (interfaces_.getLocalMac(message.target_ip, localMac))
        {
            ARP::Message reply = ARP::createReply(message, localMac, message.target_ip);

            Ethernet::EthernetHeader ethHeader;
            std::memcpy(ethHeader.destination.bytes, message.sender_mac.data(), sizeof(ethHeader.destination.bytes));
            std::memcpy(ethHeader.source.bytes, localMac.data(), sizeof(ethHeader.source.bytes));
            ethHeader.ethertype = static_cast<uint16_t>(Net::Ethernet::Type::ARP);

            if (!ARP::serialize(reply, responseBuffer, responseBufferSize))
            {
                std::cerr << "Failed to serialize ARP reply" << std::endl;
                return false;
            }

            if (!Ethernet::addEthernetHeader(responseBuffer, sizeof(ARP::Message), ethHeader))
            {
                std::cerr << "Failed to add Ethernet header to ARP reply" << std::endl;
                return false;
            }

            size_t frameSize = sizeof(Ethernet::EthernetHeader) + sizeof(ARP::Message);
            if (io_.send(responseBuffer, frameSize) < 0)
            {
                std::cerr << "Failed to send ARP reply" << std::endl;
                return false;
            }
            // Debug output
            std::cout << "Sent ARP reply to " << (int)message.sender_ip[0] << "." << (int)message.sender_ip[1] << "." << (int)message.sender_ip[2] << "." << (int)message.sender_ip[3] << std::endl;
            return true;
        }
    }
    return false;
}