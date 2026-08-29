#include "ARP.h"

namespace ARP {
    
    /**
     * Adds an entry to the ARP cache.
     * @param ip The IP address to add.
     * @param mac The MAC address associated with the IP address.
     */
    void Cache::put(const std::array<uint8_t, 4>& ip, const std::array<uint8_t, 6>& mac) {
        cache[ip] = mac;
    }

    /**
     * Retrieves the MAC address associated with the given IP address from the ARP cache.
     * @param ip The IP address to look up.
     * @return An optional containing the MAC address if found, or std::nullopt
     */
    std::optional<std::array<uint8_t, 6>> Cache::get(const std::array<uint8_t, 4>& ip) const {
        if(Cache::contains(ip)) {
            return cache.at(ip);
        }
        return std::nullopt;
    }

    /**
     * Checks if the ARP cache contains an entry for the given IP address.
     * @param ip The IP address to check.
     * @return True if the cache contains the IP address, false otherwise.
     */
    bool Cache::contains(const std::array<uint8_t, 4>& ip) const {
        return cache.find(ip) != cache.end();
    }

    /**
     * Prints the contents of the ARP cache to the standard output.
     * Each entry is printed in the format: "IP: <ip_address> MAC: <mac_address>"
     */
    void Cache::print() const {
        for (const auto& pair : cache) {
            std::cout << "IP: " << std::hex << std::setfill('0') << static_cast<int>(pair.first[0]) << "." << static_cast<int>(pair.first[1]) << "." << static_cast<int>(pair.first[2]) << "." << static_cast<int>(pair.first[3]) << " MAC: " << std::hex << std::setfill('0') << static_cast<int>(pair.second[0]) << ":" << static_cast<int>(pair.second[1]) << ":" << static_cast<int>(pair.second[2]) << ":" << static_cast<int>(pair.second[3]) << ":" << static_cast<int>(pair.second[4]) << ":" << static_cast<int>(pair.second[5]) << std::endl;
        }
    }
}