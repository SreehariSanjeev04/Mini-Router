#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>

class ArpCache
{
public:
    ArpCache() = default;

    void put(const std::array<uint8_t, 4>& ip, const std::array<uint8_t, 6>& mac);
    std::optional<std::array<uint8_t, 6>> get(const std::array<uint8_t, 4>& ip) const;
    bool contains(const std::array<uint8_t, 4>& ip) const;
    void print() const;

private:
    std::map<std::array<uint8_t, 4>, std::array<uint8_t, 6>> cache;
};