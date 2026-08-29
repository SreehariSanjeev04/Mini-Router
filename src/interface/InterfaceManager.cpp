#include "InterfaceManager.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <unistd.h>

/**
 * Constructs an InterfaceManager, opening a datagram socket used for interface ioctl queries.
 */
InterfaceManager::InterfaceManager()
    : ioctlFd_(socket(AF_INET, SOCK_DGRAM, 0))
{
}

/**
 * Closes the socket used for interface ioctl queries.
 */
InterfaceManager::~InterfaceManager()
{
    if (ioctlFd_ >= 0)
    {
        close(ioctlFd_);
    }
}

/**
 * Retrieves the MAC and IP address of the specified network interface.
 * @param interfaceName The name of the network interface (e.g., "eth0").
 * @param macAddress Reference to an array where the MAC address will be stored.
 * @param ipAddress Reference to an array where the IP address will be stored.
 * @return True if the details were successfully retrieved, false otherwise.
 */
bool InterfaceManager::getInterfaceDetails(
    const char* interfaceName,
    std::array<uint8_t, 6>& macAddress,
    std::array<uint8_t, 4>& ipAddress)
{
    struct ifreq ifr;
    std::strncpy(ifr.ifr_name, interfaceName, IFNAMSIZ - 1);
    ifr.ifr_ifrn.ifrn_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(ioctlFd_, SIOCGIFHWADDR, &ifr) == -1)
    {
        return false;
    }

    std::memcpy(macAddress.data(), ifr.ifr_hwaddr.sa_data, 6);

    if (ioctl(ioctlFd_, SIOCGIFADDR, &ifr) == -1)
    {
        return false;
    }

    std::memcpy(ipAddress.data(), &((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr, 4);
    return true;
}

/**
 * Registers a local interface in the interface map.
 * @param macAddress The MAC address of the local interface.
 * @param ipAddress The IP address of the local interface.
 * @return True if the interface was added, false otherwise.
 */
bool InterfaceManager::addLocalInterface(
    const std::array<uint8_t, 6>& macAddress,
    const std::array<uint8_t, 4>& ipAddress)
{
    return localInterfaces_.emplace(ipAddress, macAddress).second;
}

/**
 * Checks if the given IP address belongs to one of the local interfaces.
 * @param ip The IP address to check.
 * @return True if the address is a local interface, false otherwise.
 */
bool InterfaceManager::isRouterIp(const std::array<uint8_t, 4>& ip) const
{
    return localInterfaces_.find(ip) != localInterfaces_.end();
}

/**
 * Retrieves the MAC address of the local interface configured with the given IP address.
 * @param ip The IP address of the local interface.
 * @param macAddress Reference to an array where the MAC address will be stored.
 * @return True if the interface was found, false otherwise.
 */
bool InterfaceManager::getLocalMac(
    const std::array<uint8_t, 4>& ip,
    std::array<uint8_t, 6>& macAddress) const
{
    auto it = localInterfaces_.find(ip);
    if (it == localInterfaces_.end())
    {
        return false;
    }
    macAddress = it->second;
    return true;
}

/**
 * Prints the local interfaces and their addresses to the standard output.
 */
void InterfaceManager::printLocalInterfaces()
{
    std::cout << "--- Local Interfaces (" << localInterfaces_.size() << " entries) ---\n";
    for (const auto& [ip, mac] : localInterfaces_)
    {
        std::cout << (int)ip[0] << "." << (int)ip[1] << "." << (int)ip[2] << "." << (int)ip[3] << " -> ";
        for (size_t i = 0; i < 6; ++i)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)mac[i] << (i < 5 ? ":" : "");
        }
        std::cout << std::dec << "\n";
    }
    std::cout << "--- End of Local Interfaces ---\n\n";
}


/**
 * Retrieves the list of local interfaces and their details, populating the localInterfaces_ map.
 * @return True if the interfaces were successfully retrieved, false otherwise.
 */
bool InterfaceManager::getLocalInterfaces() {
    struct ifconf ifc;
    char buffer[4096];
    ifc.ifc_len = sizeof(buffer);
    ifc.ifc_buf = buffer;

    if (ioctl(ioctlFd_, SIOCGIFCONF, &ifc) == -1) {
        std::cerr << "Failed to get interface list" << std::endl;
        return false;
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        std::array<uint8_t, 6> macAddress;
        std::array<uint8_t, 4> ipAddress;

        if (getInterfaceDetails(it->ifr_name, macAddress, ipAddress)) {
            addLocalInterface(macAddress, ipAddress);
        }
    }

    return true;
}

