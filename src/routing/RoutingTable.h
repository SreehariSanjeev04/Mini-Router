/*

The routing table should contain the following information for each route:

- Next hop IP address
- Interface index through which the next hop can be reached

*/

#include <array>
#include <cstdint>

struct RouteEntry {
    std::array<uint8_t, 4> nextHopIpAddress;
    int interfaceIndex;
};

