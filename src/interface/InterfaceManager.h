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
        std::array<uint8_t, 4>& ipAddress);

    bool addLocalInterface(
        const std::string& interfaceName,
        int ifindex,
        const std::array<uint8_t, 6>& macAddress,
        const std::array<uint8_t, 4>& ipAddress);

    bool isRouterIp(const std::array<uint8_t, 4>& ip) const;

    bool getLocalMac(
        const std::array<uint8_t, 4>& ip,
        std::array<uint8_t, 6>& macAddress) const;

    bool getLocalInterfaces();

    void printLocalInterfaces();

    std::vector<int> getLocalInterfaceIndexes() const;

private:
    struct InterfaceInfo
    {
        std::array<uint8_t, 6> mac;
        std::array<uint8_t, 4> ip;
        std::string name;
    };

    int ioctlFd_;
    std::map<int, InterfaceInfo> localInterfaces_;
};