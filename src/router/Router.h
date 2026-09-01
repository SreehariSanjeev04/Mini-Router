#pragma once

#include "arp/ArpCache.h"
#include "arp/ArpHandler.h"
#include "interface/InterfaceManager.h"
#include "network/RawSocket.h"
#include "routing/IpForwarder.h"
#include "routing/RoutingTable.h"
#include "packet/ethernet/Ethernet.h"
#include "packet/ipv4/IPv4Packet.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

class Router
{
public:
    Router();

    void run();

private:
    /**
     * Handles a packet whose destination is one of the router's own IPs.
     * Currently only ICMP echo requests are answered.
     * @param packet The parsed IPv4 packet destined to the router.
     * @param senderMac The Ethernet source MAC of the received frame.
     * @param io The raw socket the packet arrived on.
     * @return True if a reply was generated and sent, false otherwise.
     */
    bool handleLocalDelivery(
        const IPv4Packet& packet,
        const Ethernet::MacAddress& senderMac,
        RawSocket& io);

    std::vector<std::unique_ptr<RawSocket>> rawSockets_;
    std::map<int, RawSocket*> socketsByIfindex_;
    InterfaceManager interfaces_;
    ArpCache arpCache_;
    ArpHandler arpHandler_;
    RoutingTable routingTable_;
    IpForwarder forwarder_;
};