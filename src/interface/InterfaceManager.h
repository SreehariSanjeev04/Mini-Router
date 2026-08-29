#pragma once
#include <array>
#include <cstdint>
#include <netinet/in.h>

bool getInterfaceDetails(
    int socketfd,
    const char *interfaceName,
    std::array<uint8_t, 6> &macAddress,
    std::array<uint8_t, 4> &ipAddress);