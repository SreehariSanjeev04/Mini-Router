#include "Router.h"

#include "commons/Constants.h"
#include "packet/ethernet/Ethernet.h"
#include "packet/icmp/ICMP.h"
#include "packet/ipv4/IPv4.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <net/ethernet.h>
#include <poll.h>
#include <utility>
#include <vector>

/**
 * Constructs the interface manager, ARP cache, ARP handler, routing table and
 * IP forwarder, wiring their dependencies. Raw sockets are opened per local
 * interface once the interfaces are discovered in run().
 */
Router::Router()
    : rawSockets_(),
      socketsByIfindex_(),
      interfaces_(),
      arpCache_(),
      arpHandler_(arpCache_, interfaces_),
      routingTable_(),
      forwarder_(routingTable_, arpCache_, interfaces_, arpHandler_, socketsByIfindex_)
{
}

/**
 * Runs the main receive loop. It opens one raw socket per local interface,
 * derives connected routes from the interfaces, then polls all sockets, parses
 * received Ethernet headers and dispatches frames to the ARP handler, the local
 * ICMP path, or the IP forwarder.
 */
void Router::run()
{
    interfaces_.getLocalInterfaces();
    interfaces_.printLocalInterfaces();

    // Derive directly connected routes from the local interfaces.
    for (const auto& iface : interfaces_.getInterfaceRoutes())
    {
        RouteEntry entry;
        entry.nextHopIpAddress = iface.ip;
        entry.subnetMask = iface.subnetMask;
        entry.interfaceIndex = iface.ifindex;
        entry.isConnected = true;
        for (size_t i = 0; i < 4; ++i)
        {
            entry.destination[i] = iface.ip[i] & iface.subnetMask[i];
        }
        routingTable_.addRoute(entry);
    }
    routingTable_.printRoutes();

    for (int ifindex : interfaces_.getLocalInterfaceIndexes())
    {
        auto socket = std::make_unique<RawSocket>(ETH_P_ALL, ifindex);
        if (socket->descriptor() == -1)
        {
            std::cerr << "Failed to create raw socket for interface index " << ifindex << std::endl;
            continue;
        }
        std::cout << "Created raw socket for interface index " << ifindex << " with fd: " << socket->descriptor() << std::endl;
        socketsByIfindex_[ifindex] = socket.get();
        rawSockets_.push_back(std::move(socket));
    }

    if (rawSockets_.empty())
    {
        std::cerr << "Failed to create any raw socket" << std::endl;
        return;
    }

    std::vector<pollfd> fds;
    fds.reserve(rawSockets_.size());
    for (const auto& socket : rawSockets_)
    {
        fds.push_back(pollfd{socket->descriptor(), POLLIN, 0});
    }

    uint8_t buffer[Net::BUFFER_SIZE];

    while (true)
    {
        int pollResult = poll(fds.data(), fds.size(), -1);
        if (pollResult < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            std::cerr << "Failed to poll raw sockets" << std::endl;
            break;
        }

        for (size_t i = 0; i < fds.size(); ++i)
        {
            if ((fds[i].revents & POLLIN) == 0)
            {
                continue;
            }

            RawSocket& socket = *rawSockets_[i];

            ssize_t numBytes = socket.receive(buffer, sizeof(buffer));
            if (numBytes < 0)
            {
                std::cerr << "Failed to receive data" << std::endl;
                continue;
            }

            std::cout << "Received " << numBytes << " bytes on interface index " << socket.interfaceIndex() << std::endl;

            Ethernet::EthernetHeader ethHeader;
            if (Ethernet::parseEthernetHeader(buffer, numBytes, ethHeader))
            {
                std::cout << "Ethernet Header:" << std::endl;
                std::cout << "Destination MAC: " << Ethernet::getMacAddressString(ethHeader.destination) << std::endl;
                std::cout << "Source MAC: " << Ethernet::getMacAddressString(ethHeader.source) << std::endl;
                std::cout << "Ethertype: 0x" << std::hex << ethHeader.ethertype << std::dec << std::endl;

                if (ethHeader.ethertype == static_cast<uint16_t>(Net::Ethernet::Type::ARP))
                {
                    // Handle ARP packet.
                    std::cout << "Received ARP packet" << std::endl;

                    uint8_t responseBuffer[Net::BUFFER_SIZE];
                    // Sender mapping is zero-initialized; it is only meaningful if
                    // the ARP message parsed successfully.
                    std::array<uint8_t, 4> senderIp{};
                    std::array<uint8_t, 6> senderMac{};
                    arpHandler_.handleARPPacket(buffer, numBytes, responseBuffer, sizeof(responseBuffer), socket, &senderIp, &senderMac);

                    // Any learned mapping may resolve queued forwarding packets.
                    forwarder_.onArpResolved(senderIp, senderMac);
                }
                else if (ethHeader.ethertype == static_cast<uint16_t>(Net::Ethernet::Type::IPv4))
                {
                    // Handle IPv4 packet.
                    std::cout << "Received IPv4 packet" << std::endl;
                    IPv4Packet ipv4Packet;
                    if (IPv4::parse(buffer + sizeof(Ethernet::EthernetHeader), numBytes - sizeof(Ethernet::EthernetHeader), ipv4Packet) &&
                        IPv4::verifyChecksum(buffer + sizeof(Ethernet::EthernetHeader), ipv4Packet.headerLength * 4))
                    {
                        IPv4::printPacket(ipv4Packet);

                        std::array<uint8_t, 4> destIp = ipv4Packet.destinationAddress.bytes;
                        if (interfaces_.isRouterIp(destIp))
                        {
                            if (!handleLocalDelivery(ipv4Packet, ethHeader.source, socket))
                            {
                                std::cout << "Packet destined to router dropped" << std::endl;
                            }
                        }
                        else
                        {
                            forwarder_.forwardPacket(ipv4Packet, socket.interfaceIndex());
                        }
                    }
                    else
                    {
                        std::cout << "Failed to parse IPv4 packet" << std::endl;
                    }
                }
                else
                {
                    std::cout << "Received non-ARP packet" << std::endl;
                }
            }
            else
            {
                std::cerr << "Failed to parse Ethernet header" << std::endl;
            }
        }
    }
}

/**
 * Answers ICMP echo requests addressed to the router and drops other traffic
 * destined to a router IP.
 * @param packet The parsed IPv4 packet destined to the router.
 * @param senderMac The Ethernet source MAC of the received frame.
 * @param io The raw socket the packet arrived on.
 * @return True if an echo reply was sent, false otherwise.
 */
bool Router::handleLocalDelivery(
    const IPv4Packet& packet,
    const Ethernet::MacAddress& senderMac,
    RawSocket& io)
{
    if (packet.protocol != static_cast<uint8_t>(Net::IPv4::Protocol::ICMP))
    {
        std::cout << "Drop: unsupported protocol " << (int)packet.protocol << " destined to router" << std::endl;
        return false;
    }

    ICMP::EchoHeader echoHeader;
    const uint8_t* echoPayload = nullptr;
    size_t echoPayloadLength = 0;
    if (!ICMP::parseEchoRequest(packet.payload, packet.payloadLength, echoHeader, echoPayload, echoPayloadLength))
    {
        std::cout << "Drop: invalid ICMP echo request" << std::endl;
        return false;
    }

    uint8_t icmpBuffer[Net::BUFFER_SIZE];
    size_t icmpLength = sizeof(ICMP::EchoHeader) + echoPayloadLength;
    if (!ICMP::createEchoReply(echoHeader, echoPayload, echoPayloadLength, icmpBuffer, sizeof(icmpBuffer)))
    {
        std::cout << "Drop: failed to build ICMP echo reply" << std::endl;
        return false;
    }

    // Build the IPv4 header for the reply (swap source/destination).
    IPv4Packet replyPacket;
    replyPacket.version = 4;
    replyPacket.headerLength = 5;
    replyPacket.tos = 0;
    replyPacket.totalLength = 20 + static_cast<uint16_t>(icmpLength);
    replyPacket.identification = 0;
    replyPacket.flags = 0;
    replyPacket.fragmentOffset = 0;
    replyPacket.ttl = 64;
    replyPacket.protocol = static_cast<uint8_t>(Net::IPv4::Protocol::ICMP);
    replyPacket.headerChecksum = 0;
    replyPacket.sourceAddress = packet.destinationAddress;
    replyPacket.destinationAddress = packet.sourceAddress;
    replyPacket.payload = icmpBuffer;
    replyPacket.payloadLength = icmpLength;

    std::vector<uint8_t> ipv4Buffer(replyPacket.totalLength);
    if (!IPv4::serialize(replyPacket, ipv4Buffer.data(), ipv4Buffer.size()))
    {
        std::cout << "Drop: failed to serialize IPv4 reply" << std::endl;
        return false;
    }
    IPv4::updateChecksum(ipv4Buffer.data(), replyPacket.headerLength * 4);

    // Wrap in an Ethernet frame addressed back to the request's sender.
    std::array<uint8_t, 6> localMac;
    std::array<uint8_t, 4> localIp;
    if (!interfaces_.getInterfaceByIndex(io.interfaceIndex(), localMac, localIp))
    {
        std::cout << "Drop: unknown interface " << io.interfaceIndex() << std::endl;
        return false;
    }

    std::vector<uint8_t> frame(ipv4Buffer.size() + sizeof(Ethernet::EthernetHeader));
    std::memcpy(frame.data() + sizeof(Ethernet::EthernetHeader), ipv4Buffer.data(), ipv4Buffer.size());

    Ethernet::EthernetHeader ethHeader;
    std::memcpy(ethHeader.destination.bytes, senderMac.bytes, sizeof(ethHeader.destination.bytes));
    std::memcpy(ethHeader.source.bytes, localMac.data(), sizeof(ethHeader.source.bytes));
    ethHeader.ethertype = static_cast<uint16_t>(Net::Ethernet::Type::IPv4);

    if (!Ethernet::serialize(ethHeader, frame.data(), frame.size()))
    {
        std::cout << "Drop: failed to serialize Ethernet header" << std::endl;
        return false;
    }

    if (io.send(frame.data(), frame.size()) < 0)
    {
        std::cout << "Failed to send ICMP echo reply" << std::endl;
        return false;
    }

    std::cout << "Sent ICMP echo reply to "
              << (int)packet.sourceAddress.bytes[0] << "." << (int)packet.sourceAddress.bytes[1] << "."
              << (int)packet.sourceAddress.bytes[2] << "." << (int)packet.sourceAddress.bytes[3] << std::endl;
    return true;
}