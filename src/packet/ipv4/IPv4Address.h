#pragma once
#pragma pack(push, 1)
#include <cstdint>
#include <cstring>
#include <array>
struct IPv4Address {

    std::array<uint8_t, 4> bytes;

    bool operator==(const IPv4Address& other) const {
        return bytes == other.bytes;
    }

    bool operator!=(const IPv4Address& other) const {
        return bytes != other.bytes;
    }
};

#pragma pack(pop)