#pragma once
#pragma pack(push, 1)
#include <cstdint>
#include <cstring>

struct IPv4Address {

    uint8_t bytes[4];

    bool operator==(const IPv4Address& other) const {
        return std::memcmp(bytes, other.bytes, sizeof(bytes)) == 0;
    }

    bool operator!=(const IPv4Address& other) const {
        return std::memcmp(bytes, other.bytes, sizeof(bytes)) != 0;
    }
};

#pragma pack(pop)