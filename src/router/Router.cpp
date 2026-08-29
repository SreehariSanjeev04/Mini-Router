#include "Router.h"

#include "commons/Constants.h"
#include "packet/ethernet/Ethernet.h"

#include <iostream>
#include <net/ethernet.h>

/**
 * Constructs the raw socket, interface manager, ARP cache, and ARP handler,
 * wiring their dependencies.
 */
Router::Router()
    : rawSocket_(ETH_P_ALL),
      interfaces_(),
      arpCache_(),
      arpHandler_(arpCache_, interfaces_, rawSocket_)
{
}

/**
 * Runs the main receive loop. It receives frames, parses the Ethernet header,
 * and dispatches ARP packets to the ARP handler.
 */
void Router::run()
{
    interfaces_.getLocalInterfaces();
    interfaces_.printLocalInterfaces();

    if (rawSocket_.descriptor() == -1)
    {
        std::cerr << "Failed to create raw socket" << std::endl;
        return;
    }

    std::cout << "Raw socket created successfully with fd: " << rawSocket_.descriptor() << std::endl;

    uint8_t buffer[Net::BUFFER_SIZE];

    while (true)
    {
        ssize_t numBytes = rawSocket_.receive(buffer, sizeof(buffer));

        if (numBytes < 0)
        {
            std::cerr << "Failed to receive data" << std::endl;
            break;
        }

        std::cout << "Received " << numBytes << " bytes" << std::endl;

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
                arpHandler_.handleARPPacket(buffer, numBytes, responseBuffer, sizeof(responseBuffer));
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