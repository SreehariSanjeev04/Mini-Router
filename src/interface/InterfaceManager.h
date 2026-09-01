#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class InterfaceManager
{
public:
    InterfaceManager();
    ~InterfaceManager();

    InterfaceManager(const InterfaceManager&) = delete;
    InterfaceManager& operator=(const InterfaceManager&) = delete;

    bool getInterfaceDetails(
        const char* interfaceName,
        std::array<uint8_t, 6>& macAddress,
        std::array<uint8_t, 4>& ipAddress,
        std::array<uint8_t, 4>& subnetMask);

    bool addLocalInterface(
        const std::string& interfaceName,
        int ifindex,
        const std::array<uint8_t, 6>& macAddress,
        const std::array<uint8_t, 4>& ipAddress,
        const std::array<uint8_t, 4>& subnetMask);

    bool isRouterIp(const std::array<uint8_t, 4>& ip) const;

    bool getLocalMac(
        const std::array<uint8_t, 4>& ip,
        std::array<uint8_t, 6>& macAddress) const;

    bool getInterfaceByIndex(
        int ifindex,
        std::array<uint8_t, 6>& macAddress,
        std::array<uint8_t, 4>& ipAddress) const;

    bool getLocalInterfaces();

    void printLocalInterfaces();

    std::vector<int> getLocalInterfaceIndexes() const;

    /** Subset of interface data needed to build connected routes. */
    struct InterfaceRouteInfo
    {
        std::array<uint8_t, 4> ip;
        std::array<uint8_t, 4> subnetMask;
        int ifindex;
    };

    std::vector<InterfaceRouteInfo> getInterfaceRoutes() const;

private:
    struct InterfaceInfo
    {
        std::array<uint8_t, 6> mac;
        std::array<uint8_t, 4> ip;
        std::array<uint8_t, 4> subnetMask;
        std::string name;
    };

    int ioctlFd_;
    std::map<int, InterfaceInfo> localInterfaces_;
};