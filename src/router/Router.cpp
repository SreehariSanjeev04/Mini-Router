#include "Router.h"

#include "commons/Constants.h"
#include "packet/ethernet/Ethernet.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <net/ethernet.h>
#include <poll.h>
#include <utility>
#include <vector>

/**
 * Constructs the interface manager, ARP cache, and ARP handler, wiring their dependencies.
 * Raw sockets are opened per local interface once the interfaces are discovered in run().
 */
Router::Router()
    : rawSockets_(),
      interfaces_(),
      arpCache_(),
      arpHandler_(arpCache_, interfaces_)
{
}

/**
 * Runs the main receive loop. It opens one raw socket per local interface, then polls
 * all sockets, parses received Ethernet headers, and dispatches ARP packets to the ARP
 * handler together with the interface the frame arrived on.
 */
void Router::run()
{
    interfaces_.getLocalInterfaces();
    interfaces_.printLocalInterfaces();

    for (int ifindex : interfaces_.getLocalInterfaceIndexes())
    {
        auto socket = std::make_unique<RawSocket>(ETH_P_ALL, ifindex);
        if (socket->descriptor() == -1)
        {
            std::cerr << "Failed to create raw socket for interface index " << ifindex << std::endl;
            continue;
        }
        std::cout << "Created raw socket for interface index " << ifindex << " with fd: " << socket->descriptor() << std::endl;
        rawSockets_.push_back(std::move(socket));
    }

    if (rawSockets_.empty())
    {
        std::cerr << "Failed to create any raw socket" << std::endl;
        return;
    }

    std::vector<pollfd> fds;
    fds.reserve(rawSockets_.size());
    for (const auto& socket : rawSockets_)
    {
        fds.push_back(pollfd{socket->descriptor(), POLLIN, 0});
    }

    uint8_t buffer[Net::BUFFER_SIZE];

    while (true)
    {
        int pollResult = poll(fds.data(), fds.size(), -1);
        if (pollResult < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            std::cerr << "Failed to poll raw sockets" << std::endl;
            break;
        }

        for (size_t i = 0; i < fds.size(); ++i)
        {
            if ((fds[i].revents & POLLIN) == 0)
            {
                continue;
            }

            RawSocket& socket = *rawSockets_[i];

            ssize_t numBytes = socket.receive(buffer, sizeof(buffer));
            if (numBytes < 0)
            {
                std::cerr << "Failed to receive data" << std::endl;
                continue;
            }

            std::cout << "Received " << numBytes << " bytes on interface index " << socket.interfaceIndex() << std::endl;

            Ethernet::EthernetHeader ethHeader;
            if (Ethernet::parseEthernetHeader(buffer, numBytes, ethHeader))
            {
                std::cout << "Ethernet Header:" << std::endl;
                std::cout << "Destination MAC: " << Ethernet::getMacAddressString(ethHeader.destination) << std::endl;
                std::cout << "Source MAC: " << Ethernet::getMacAddressString(ethHeader.source) << std::endl;
                std::cout << "Ethertype: 0x" << std::hex << ethHeader.ethertype << std::dec << std::endl;

                if (ethHeader.ethertype == static_cast<uint16_t>(Net::Ethernet::Type::ARP))
                {
                    std::cout << "Received ARP packet" << std::endl;

                    uint8_t responseBuffer[Net::BUFFER_SIZE];
                    arpHandler_.handleARPPacket(buffer, numBytes, responseBuffer, sizeof(responseBuffer), socket);
                }
                else
                {
                    std::cout << "Received non-ARP packet" << std::endl;
                }
            }
            else
            {
                std::cerr << "Failed to parse Ethernet header" << std::endl;
            }
        }
    }
}