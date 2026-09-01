#include "RoutingTable.h"

#include <iostream>

/**
 * Adds a route to the table.
 * @param route The route to add.
 */
void RoutingTable::addRoute(const RouteEntry& route)
{
    routes.push_back(route);
}

/**
 * Finds the most specific route whose destination subnet matches the given IP.
 * Longest-prefix matching: among all routes where (destinationIp & subnetMask)
 * equals the route's destination, the one with the largest subnet mask wins.
 * @param destinationIp The IP address to look up.
 * @return The best matching RouteEntry, or std::nullopt if none match.
 */
std::optional<RouteEntry> RoutingTable::findRoute(const std::array<uint8_t, 4>& destinationIp) const
{
    const RouteEntry* best = nullptr;
    uint32_t bestMaskBits = 0;

    for (const auto& route : routes)
    {
        bool matches = true;
        for (size_t i = 0; i < 4; ++i)
        {
            if ((destinationIp[i] & route.subnetMask[i]) != route.destination[i])
            {
                matches = false;
                break;
            }
        }
        if (!matches)
        {
            continue;
        }

        uint32_t maskBits = 0;
        for (size_t i = 0; i < 4; ++i)
        {
            maskBits += static_cast<uint32_t>(__builtin_popcount(route.subnetMask[i]));
        }

        if (best == nullptr || maskBits > bestMaskBits)
        {
            best = &route;
            bestMaskBits = maskBits;
        }
    }

    if (best == nullptr)
    {
        return std::nullopt;
    }
    return *best;
}

/**
 * Checks whether the routing table contains any routes.
 * @return True if the table is empty, false otherwise.
 */
bool RoutingTable::isEmpty() const
{
    return routes.empty();
}

/**
 * Prints all routes in the table to standard output.
 */
void RoutingTable::printRoutes() const
{
    std::cout << "--- Routing Table (" << routes.size() << " entries) ---\n";
    for (const auto& route : routes)
    {
        std::cout << (int)route.destination[0] << "." << (int)route.destination[1] << "."
                  << (int)route.destination[2] << "." << (int)route.destination[3] << "/"
                  << (int)route.subnetMask[0] << "." << (int)route.subnetMask[1] << "."
                  << (int)route.subnetMask[2] << "." << (int)route.subnetMask[3]
                  << " via ";
        if (route.isConnected)
        {
            std::cout << "connected";
        }
        else
        {
            std::cout << (int)route.nextHopIpAddress[0] << "." << (int)route.nextHopIpAddress[1] << "."
                      << (int)route.nextHopIpAddress[2] << "." << (int)route.nextHopIpAddress[3];
        }
        std::cout << " (interface " << route.interfaceIndex << ")\n";
    }
    std::cout << "--- End of Routing Table ---\n\n";
}