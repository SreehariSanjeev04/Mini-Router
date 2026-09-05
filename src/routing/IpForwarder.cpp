#include "IpForwarder.h"

#include "arp/ArpCache.h"
#include "arp/ArpHandler.h"
#include "commons/Constants.h"
#include "interface/InterfaceManager.h"
#include "network/RawSocket.h"
#include "packet/ethernet/Ethernet.h"
#include "packet/ipv4/IPv4.h"
#include "routing/RoutingTable.h"
#include "icmp/ICMP.h"

#include <iostream>
#include <cstring>

/**
 * Constructs the forwarder with its dependencies.
 * @param routes The routing table used for destination lookups.
 * @param arpCache The ARP cache used for next-hop MAC resolution.
 * @param interfaces Interface manager for outbound interface MAC addresses.
 * @param arpHandler Used to issue directed ARP requests on cache misses.
 * @param socketsByIfindex Map from interface index to the bound raw socket.
 */
IpForwarder::IpForwarder(
    RoutingTable& routes,
    ArpCache& arpCache,
    InterfaceManager& interfaces,
    ArpHandler& arpHandler,
    std::map<int, RawSocket*>& socketsByIfindex)
    : routes_(routes),
      arpCache_(arpCache),
      interfaces_(interfaces),
      arpHandler_(arpHandler),
      socketsByIfindex_(socketsByIfindex)
{
}

/**
 * Forwards a parsed IPv4 packet toward its destination.
 * The packet is serialized with a decremented TTL and a refreshed checksum, then
 * either sent immediately (if the next-hop MAC is already cached) or queued while
 * a directed ARP request resolves the next-hop MAC.
 * @param packet The parsed IPv4 packet.
 */
void IpForwarder::forwardPacket(const IPv4Packet& packet)
{
    std::array<uint8_t, 4> dest = IPv4::destinationAddress(packet);

    if (packet.header.ttl <= 1)
    {
        std::cout << "Drop IPv4: TTL expired for " << IPv4::ipToString(dest) << std::endl;
        return;
    }

    auto route = routes_.findRoute(dest);
    if (!route)
    {
        std::cout << "Drop IPv4: no route for " << IPv4::ipToString(dest) << std::endl;
        
        // send icmp unreachable
        const uint8_t headerLength = IPv4::headerLengthBytes(packet);
        uint8_t* buffer;
        if(!ICMP::createICMPErrorReply(packet, headerLength, static_cast<const uint8_t>(Net::ICMP::Code::DEST_UNREACHABLE), static_cast<const uint8_t>(Net::ICMP::Code::DEST_UNREACHABLE), buffer));

        IPv4Packet newPacket;
        
    }

    // very doubtful for why there isConnected :(
    // For directly connected routes the next hop is on the same L2 segment, so
    // the ARP target is the final destination. Otherwise ARP for the gateway.
    std::array<uint8_t, 4> arpTarget = route->isConnected ? dest : route->nextHopIpAddress;

    // Build the outgoing packet with TTL decremented and checksum refreshed.
    IPv4Packet outgoing = packet;
    outgoing.header.ttl -= 1;

    std::vector<uint8_t> outgoingBytes(IPv4::totalLength(outgoing));
    if (!IPv4::serialize(outgoing, outgoingBytes.data(), outgoingBytes.size()))
    {
        std::cout << "Drop IPv4: failed to serialize outgoing packet" << std::endl;
        return;
    }
    IPv4::updateChecksum(outgoingBytes.data(), IPv4::headerLengthBytes(outgoing));

    int outIfindex = route->interfaceIndex;

    if (auto mac = arpCache_.get(arpTarget))
    {
        if (sendPacket(outgoingBytes, *mac, outIfindex))
        {
            std::cout << "Forwarded IPv4 packet to " << IPv4::ipToString(dest)
                      << " via interface " << outIfindex << std::endl;
        }
        return;
    }

    // Next-hop MAC unknown: issue a directed ARP request and queue the packet.
    auto& queue = pending_[arpTarget];
    if (queue.size() < MAX_PENDING_PER_TARGET)
    {
        queue.push_back(QueuedPacket{std::move(outgoingBytes), outIfindex});
    }
    else
    {
        std::cout << "Drop IPv4: ARP resolution queue full for "
                  << IPv4::ipToString(arpTarget) << std::endl;
        return;
    }

    auto socketIt = socketsByIfindex_.find(outIfindex);
    if (socketIt == socketsByIfindex_.end())
    {
        std::cout << "Drop IPv4: no socket for outgoing interface " << outIfindex << std::endl;
        pending_[arpTarget].pop_back();
        return;
    }

    arpHandler_.sendArpRequest(arpTarget, *socketIt->second);
}

/**
 * Flushes queued packets once their next-hop IP has been resolved.
 * @param ip The ARP cache key that has been resolved.
 * @param mac The MAC address learned for that IP.
 */
void IpForwarder::onArpResolved(
    const std::array<uint8_t, 4>& ip,
    const std::array<uint8_t, 6>& mac)
{
    auto it = pending_.find(ip);
    if (it == pending_.end())
    {
        return;
    }

    auto packets = std::move(it->second);
    pending_.erase(it);

    for (const auto& queued : packets)
    {
        if (sendPacket(queued.ipv4Bytes, mac, queued.outInterfaceIndex))
        {
            std::cout << "Forwarded queued IPv4 packet to "
                      << (int)ip[0] << "." << (int)ip[1] << "."
                      << (int)ip[2] << "." << (int)ip[3]
                      << " via interface " << queued.outInterfaceIndex << std::endl;
        }
    }
}

/**
 * Prepends a fresh Ethernet header (src = outbound interface MAC, dst = next-hop
 * MAC) and sends the frame out through the raw socket of the outbound interface.
 * @param ipv4Bytes Serialized IPv4 packet including payload.
 * @param dstMac Destination MAC for the Ethernet header.
 * @param outInterfaceIndex Index of the outbound interface.
 * @return True if the frame was sent successfully.
 */
bool IpForwarder::sendPacket(
    const std::vector<uint8_t>& ipv4Bytes,
    const std::array<uint8_t, 6>& dstMac,
    int outInterfaceIndex)
{
    auto socketIt = socketsByIfindex_.find(outInterfaceIndex);
    if (socketIt == socketsByIfindex_.end())
    {
        std::cout << "Drop IPv4: no socket for outgoing interface " << outInterfaceIndex << std::endl;
        return false;
    }

    std::array<uint8_t, 6> localMac;
    std::array<uint8_t, 4> localIp;
    if (!interfaces_.getInterfaceByIndex(outInterfaceIndex, localMac, localIp))
    {
        std::cout << "Drop IPv4: unknown outgoing interface " << outInterfaceIndex << std::endl;
        return false;
    }

    std::vector<uint8_t> frame(ipv4Bytes.size() + sizeof(Ethernet::EthernetHeader));
    std::memcpy(frame.data() + sizeof(Ethernet::EthernetHeader), ipv4Bytes.data(), ipv4Bytes.size());

    Ethernet::EthernetHeader ethHeader;
    std::memcpy(ethHeader.destination.bytes, dstMac.data(), sizeof(ethHeader.destination.bytes));
    std::memcpy(ethHeader.source.bytes, localMac.data(), sizeof(ethHeader.source.bytes));
    ethHeader.ethertype = static_cast<uint16_t>(Net::Ethernet::Type::IPv4);

    if (!Ethernet::serialize(ethHeader, frame.data(), frame.size()))
    {
        std::cout << "Drop IPv4: failed to serialize Ethernet header" << std::endl;
        return false;
    }

    if (socketIt->second->send(frame.data(), frame.size()) < 0)
    {
        std::cout << "Drop IPv4: failed to send frame" << std::endl;
        return false;
    }
    return true;
}