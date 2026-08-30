#pragma once

#include <cstddef>
#include <cstdint>

class ArpCache;
class InterfaceManager;
class RawSocket;

class ArpHandler
{
public:
    ArpHandler(
        ArpCache& cache,
        InterfaceManager& interfaces);

    bool handleARPPacket(
        const uint8_t* data,
        size_t length,
        uint8_t* responseBuffer,
        size_t responseBufferSize,
        RawSocket& io);

private:
    ArpCache& cache_;
    InterfaceManager& interfaces_;
};