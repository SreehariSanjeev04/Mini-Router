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
        InterfaceManager& interfaces,
        RawSocket& io,
        bool enableProxyARP = true);

    bool handleARPPacket(
        const uint8_t* data,
        size_t length,
        uint8_t* responseBuffer,
        size_t responseBufferSize);

private:
    ArpCache& cache_;
    InterfaceManager& interfaces_;
    RawSocket& io_;
    bool enableProxyARP_ = true;
};