#include "RawSocket.h"

#include <arpa/inet.h>
#include <cstring>
#include <linux/if_packet.h>
#include <net/ethernet.h>

/**
 * Creates a raw socket for the given protocol and binds it to the given interface.
 * @param protocol The protocol used for filtering received frames (e.g., ETH_P_ALL).
 * @param interfaceIndex The index of the interface this socket is bound to.
 */
RawSocket::RawSocket(int protocol, int interfaceIndex)
    : fd_(socket(AF_PACKET, SOCK_RAW, htons(protocol))),
      protocol_(protocol),
      interfaceIndex_(interfaceIndex)
{
    if (fd_ < 0)
    {
        return;
    }

    struct sockaddr_ll addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(protocol);
    addr.sll_ifindex = interfaceIndex; // binding the socket to the specified interface

    if (bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        close(fd_);
        fd_ = -1;
    }
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
 * Returns the index of the interface this socket is bound to.
 * @return The interface index.
 */
int RawSocket::interfaceIndex() const
{
    return interfaceIndex_;
}

/**
 * Receives data from the raw socket.
 * @param buffer Pointer to the buffer where the received data will be stored.
 * @param length Size of the buffer.
 * @return The number of bytes received, or -1 on failure.
 */
ssize_t RawSocket::receive(void* buffer, size_t length)
{
    return recvfrom(fd_, buffer, length, 0, nullptr, nullptr);
}

/**
 * Sends data through the raw socket on the interface it is bound to.
 * @param buffer Pointer to the buffer containing the data to send.
 * @param length Size of the data to send.
 * @return The number of bytes sent, or -1 on failure.
 */
ssize_t RawSocket::send(const void* buffer, size_t length)
{
    struct sockaddr_ll addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(protocol_);
    addr.sll_ifindex = interfaceIndex_;

    return sendto(fd_, buffer, length, 0, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
}