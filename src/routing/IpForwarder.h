#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include "packet/ipv4/IPv4Packet.h"

class ArpCache;
class ArpHandler;
class InterfaceManager;
class RawSocket;
class RoutingTable;

/**
 * Handles IPv4 forwarding: looks up a route for the destination, decrements
 * the TTL, resolves the next-hop MAC (issuing a directed ARP request when
 * needed and queueing the packet until the reply arrives), rebuilds the
 * Ethernet header and sends the frame out on the correct interface.
 */
class IpForwarder
{
public:
    IpForwarder(
        RoutingTable& routes,
        ArpCache& arpCache,
        InterfaceManager& interfaces,
        ArpHandler& arpHandler,
        std::map<int, RawSocket*>& socketsByIfindex);

    /**
     * Attempts to forward a received IPv4 packet toward its destination.
     * @param packet The parsed IPv4 packet (payload points into the RX buffer).
     */
    void forwardPacket(const IPv4Packet& packet);

    /**
     * Called when the ARP cache learns a new (ip -> mac) mapping. Flushes any
     * packets that were queued waiting for that IP to be resolved.
     * @param ip The resolved IP address.
     * @param mac The MAC address bound to that IP.
     */
    void onArpResolved(
        const std::array<uint8_t, 4>& ip,
        const std::array<uint8_t, 6>& mac);

private:
    /** A packet held back until the next-hop MAC is resolved. */
    struct QueuedPacket
    {
        std::vector<uint8_t> ipv4Bytes; // serialized IPv4 packet incl. payload
        int outInterfaceIndex;
    };

    bool sendPacket(
        const std::vector<uint8_t>& ipv4Bytes,
        const std::array<uint8_t, 6>& dstMac,
        int outInterfaceIndex);

    RoutingTable& routes_;
    ArpCache& arpCache_;
    InterfaceManager& interfaces_;
    ArpHandler& arpHandler_;
    std::map<int, RawSocket*>& socketsByIfindex_;

    // Pending packets keyed by the IP we are trying to resolve.
    std::map<std::array<uint8_t, 4>, std::vector<QueuedPacket>> pending_;

    static constexpr size_t MAX_PENDING_PER_TARGET = 32;
};