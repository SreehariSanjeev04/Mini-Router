#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <cstring>
#include <netinet/in.h>

#include "InterfaceManager.h"

namespace InterfaceManager
{
    /**
     * Retrieves the MAC and IP address of the specified network interface.
     * @param socketfd The file descriptor of the socket.
     * @param interfaceName The name of the network interface (e.g., "eth0").
     * @param macAddress Reference to an array where the MAC address will be stored.
     * @param ipAddress Reference to an array where the IP address will be stored.
     * @return True if the details were successfully retrieved, false otherwise.
     */
    bool getInterfaceDetails(
        int socketfd,
        const char *interfaceName,
        std::array<uint8_t, 6> &macAddress,
        std::array<uint8_t, 4> &ipAddress)
    {
        struct ifreq ifr;
        std::strncpy(ifr.ifr_name, interfaceName, IFNAMSIZ - 1);
        ifr.ifr_ifrn.ifrn_name[IFNAMSIZ - 1] = '\0';

        if (ioctl(socketfd, SIOCGIFHWADDR, &ifr) == -1)
        {

            return false;
        }

        std::memcpy(macAddress.data(), ifr.ifr_hwaddr.sa_data, 6);

        if (ioctl(socketfd, SIOCGIFADDR, &ifr) == -1)
        {
            return false;
        }

        std::memcpy(ipAddress.data(), &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, 4);
        return true;
    }
}