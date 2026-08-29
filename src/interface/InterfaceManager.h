#pragma once
#include <array>
#include <cstdint>
#include <map>

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
        const std::array<uint8_t, 6>& macAddress,
        const std::array<uint8_t, 4>& ipAddress);

    bool isRouterIp(const std::array<uint8_t, 4>& ip) const;

    bool getLocalMac(
        const std::array<uint8_t, 4>& ip,
        std::array<uint8_t, 6>& macAddress) const;

    bool getLocalInterfaces();

    void printLocalInterfaces();

private:
    int ioctlFd_;
    std::map<std::array<uint8_t, 4>, std::array<uint8_t, 6>> localInterfaces_;
};