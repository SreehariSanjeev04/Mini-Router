#include "RawSocket.h"

#include <arpa/inet.h>
#include <cstring>
#include <linux/if_packet.h>
#include <net/ethernet.h>

/**
 * Creates a raw socket for the given protocol.
 * @param protocol The protocol used for filtering received frames (e.g., ETH_P_ALL).
 */
RawSocket::RawSocket(int protocol)
    : fd_(socket(AF_PACKET, SOCK_RAW, htons(protocol))),
      lastInterfaceIndex_(0)
{
}

/**
 * Closes the raw socket.
 */
RawSocket::~RawSocket()
{
    if (fd_ >= 0)
    {
        close(fd_);
    }
}

/**
 * Returns the underlying socket file descriptor.
 * @return The socket file descriptor, or -1 if the socket could not be created.
 */
int RawSocket::descriptor() const
{
    return fd_;
}

/**
 * Receives data from the raw socket and records the interface it arrived on.
 * @param buffer Pointer to the buffer where the received data will be stored.
 * @param length Size of the buffer.
 * @return The number of bytes received, or -1 on failure.
 */
ssize_t RawSocket::receive(void* buffer, size_t length)
{
    struct sockaddr_ll addr;
    socklen_t addrLen = sizeof(addr);

    ssize_t numBytes = recvfrom(fd_, buffer, length, 0, reinterpret_cast<struct sockaddr*>(&addr), &addrLen);
    if (numBytes >= 0)
    {
        lastInterfaceIndex_ = addr.sll_ifindex;
    }
    return numBytes;
}

/**
 * Sends data through the raw socket on the interface frames were last received from.
 * @param buffer Pointer to the buffer containing the data to send.
 * @param length Size of the data to send.
 * @return The number of bytes sent, or -1 on failure.
 */
ssize_t RawSocket::send(const void* buffer, size_t length)
{
    struct sockaddr_ll addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ARP);
    addr.sll_ifindex = lastInterfaceIndex_;

    return sendto(fd_, buffer, length, 0, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
}   