#include "ARP.h"
#include <arpa/inet.h>
#include <iostream>
#include <iomanip>

namespace ARP
{

    /**
     * Adds an entry to the ARP cache.
     * @param ip The IP address to add.
     * @param mac The MAC address associated with the IP address.
     */
    void Cache::put(const std::array<uint8_t, 4> &ip, const std::array<uint8_t, 6> &mac)
    {
        cache[ip] = mac;
    }

    /**
     * Retrieves the MAC address associated with the given IP address from the ARP cache.
     * @param ip The IP address to look up.
     * @return An optional containing the MAC address if found, or std::nullopt
     */
    std::optional<std::array<uint8_t, 6>> Cache::get(const std::array<uint8_t, 4> &ip) const
    {
        if (Cache::contains(ip))
        {
            return cache.at(ip);
        }
        return std::nullopt;
    }

    /**
     * Checks if the ARP cache contains an entry for the given IP address.
     * @param ip The IP address to check.
     * @return True if the cache contains the IP address, false otherwise.
     */
    bool Cache::contains(const std::array<uint8_t, 4> &ip) const
    {
        return cache.find(ip) != cache.end();
    }

    /**
     * Prints the contents of the ARP cache to the standard output.
     * Each entry is printed in the format: "IP: <ip_address> MAC: <mac_address>"
     */
    void Cache::print() const
    {
        std::cout << "--- ARP Cache (" << cache.size() << " entries) ---\n";
        for (const auto &[ip, mac] : cache)
        {
            std::cout << (int)ip[0] << "." << (int)ip[1] << "." << (int)ip[2] << "." << (int)ip[3] << " -> ";
            for (size_t i = 0; i < 6; ++i)
            {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)mac[i] << (i < 5 ? ":" : "");
            }
            std::cout << std::dec << "\n";
        }
    }
}