#pragma once

#include <array>
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

    /**
     * Handles an incoming ARP packet: updates the ARP cache with the sender
     * mapping and answers requests targeting a router interface.
     * @param data The raw packet data.
     * @param length The length of the packet data.
     * @param responseBuffer A buffer to store the generated ARP reply frame, if any.
     * @param responseBufferSize The size of the response buffer.
     * @param io The raw socket bound to the interface the frame arrived on.
     * @param senderIpOut If non-null, filled with the sender IP of the parsed ARP message.
     * @param senderMacOut If non-null, filled with the sender MAC of the parsed ARP message.
     * @return True if a reply was generated and sent, false otherwise.
     */
    bool handleARPPacket(
        const uint8_t* data,
        size_t length,
        uint8_t* responseBuffer,
        size_t responseBufferSize,
        RawSocket& io,
        std::array<uint8_t, 4>* senderIpOut = nullptr,
        std::array<uint8_t, 6>* senderMacOut = nullptr);

    /**
     * Sends an ARP request for the given target IP on the interface the socket
     * is bound to. The request is sent to the broadcast MAC address.
     * @param targetIp The IP address to resolve.
     * @param io The raw socket bound to the outbound interface.
     * @return True if the request was sent successfully.
     */
    bool sendArpRequest(
        const std::array<uint8_t, 4>& targetIp,
        RawSocket& io);

private:
    ArpCache& cache_;
    InterfaceManager& interfaces_;
};