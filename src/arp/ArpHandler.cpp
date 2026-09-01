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
 */
ArpHandler::ArpHandler(
    ArpCache &cache,
    InterfaceManager &interfaces)
    : cache_(cache),
      interfaces_(interfaces)
{
}

/**
 * Handles an incoming ARP packet. It parses the packet, updates the ARP cache, and if the
 * request targets one of the router's interfaces, builds an Ethernet frame containing the ARP
 * reply and sends it on the interface the request arrived on.
 * @param data The raw packet data.
 * @param length The length of the packet data.
 * @param responseBuffer A buffer to store the generated ARP reply frame, if any.
 * @param responseBufferSize The size of the response buffer.
 * @param io The raw socket bound to the interface the request arrived on.
 * @return True if a reply was generated and sent, false otherwise.
 */
bool ArpHandler::handleARPPacket(
    const uint8_t *data,
    size_t length,
    uint8_t *responseBuffer,
    size_t responseBufferSize,
    RawSocket &io,
    std::array<uint8_t, 4> *senderIpOut,
    std::array<uint8_t, 6> *senderMacOut)
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

    // Expose the sender mapping so the caller can, e.g., flush queued forwards.
    if (senderIpOut != nullptr)
    {
        *senderIpOut = message.sender_ip;
    }
    if (senderMacOut != nullptr)
    {
        *senderMacOut = message.sender_mac;
    }

    if (ARP::isRequest(message))
    {

        if (interfaces_.isRouterIp(message.target_ip))
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
                if (io.send(responseBuffer, frameSize) < 0)
                {
                    std::cerr << "Failed to send ARP reply" << std::endl;
                    return false;
                }
                // debug output
                std::cout << "Sent ARP reply to " << (int)message.sender_ip[0] << "." << (int)message.sender_ip[1] << "." << (int)message.sender_ip[2] << "." << (int)message.sender_ip[3] << std::endl;
                return true;
            }
        }
    }
    return false;
}

bool ArpHandler::sendArpRequest(
    const std::array<uint8_t, 4> &targetIp,
    RawSocket &io)
{
    std::array<uint8_t, 6> localMac;
    std::array<uint8_t, 4> localIp;
    if (!interfaces_.getInterfaceByIndex(io.interfaceIndex(), localMac, localIp))
    {
        std::cerr << "Failed to find local interface information for request" << std::endl;
        return false;
    }

    ARP::Message request;
    request.hardware_type = Net::ARP::HARDWARE_ETHERNET;
    request.protocol_type = static_cast<uint16_t>(Net::Ethernet::Type::IPv4);
    request.hardware_size = Net::ARP::HARDWARE_LENGTH;
    request.protocol_size = Net::ARP::PROTOCOL_LENGTH;
    request.opcode = static_cast<uint16_t>(Net::ARP::Opcode::Request);
    request.sender_mac = localMac;
    request.sender_ip = localIp;
    request.target_mac.fill(0);
    request.target_ip = targetIp;

    uint8_t frame[Net::BUFFER_SIZE];

    if (!ARP::serialize(request, frame, sizeof(frame)))
    {
        std::cerr << "Failed to serialize ARP request" << std::endl;
        return false;
    }

    Ethernet::EthernetHeader ethHeader;
    std::memset(ethHeader.destination.bytes, 0xFF, sizeof(ethHeader.destination.bytes));
    std::memcpy(ethHeader.source.bytes, localMac.data(), sizeof(ethHeader.source.bytes));
    ethHeader.ethertype = static_cast<uint16_t>(Net::Ethernet::Type::ARP);

    if (!Ethernet::addEthernetHeader(frame, sizeof(ARP::Message), ethHeader))
    {
        std::cerr << "Failed to add Ethernet header to ARP request" << std::endl;
        return false;
    }

    size_t frameSize = sizeof(Ethernet::EthernetHeader) + sizeof(ARP::Message);
    if (io.send(frame, frameSize) < 0)
    {
        std::cerr << "Failed to send ARP request" << std::endl;
        return false;
    }

    std::cout << "Sent ARP request for "
              << (int)targetIp[0] << "." << (int)targetIp[1] << "."
              << (int)targetIp[2] << "." << (int)targetIp[3] << std::endl;
    return true;
}