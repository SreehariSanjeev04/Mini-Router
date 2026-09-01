#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

/**
 * A single entry in the routing table.
 * For directly connected subnets, isConnected is true and nextHopIpAddress is
 * the router's own interface IP (the ARP target then becomes the final
 * destination IP, since both hosts share the same L2 segment).
 */
struct RouteEntry {
    std::array<uint8_t, 4> destination;
    std::array<uint8_t, 4> subnetMask;
    std::array<uint8_t, 4> nextHopIpAddress;
    int interfaceIndex;
    bool isConnected;
};

/**
 * A basic IPv4 routing table. Routes are matched with a longest-prefix match:
 * the route whose mask is most specific and whose (dest & mask) equals the
 * entry's destination wins.
 */
class RoutingTable {
public:
    void addRoute(const RouteEntry& route);


    std::optional<RouteEntry> findRoute(const std::array<uint8_t, 4>& destinationIp) const;

    bool isEmpty() const;

    void printRoutes() const;

private:
    std::vector<RouteEntry> routes;
};