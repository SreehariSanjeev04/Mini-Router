#include <cstdint>
#include <iostream>

#include <net/ethernet.h>
#include <sys/socket.h>

#include "commons/Constants.h"
#include "network/RawSocket.h"
#include "packet/ethernet/Ethernet.h"

int main() {

  int socketfd = Socket::createRawSocket(ETH_P_ALL);

  if (socketfd == -1) {
    std::cerr << "Failed to create raw socket" << std::endl;
    return 1;
  }

  std::cout << "Raw socket created successfully with fd: " << socketfd << std::endl;

  uint8_t buffer[Net::Ethernet::BUFFER_SIZE];

  while(true) {
    ssize_t numBytes = recvfrom(socketfd, buffer, sizeof(buffer), 0, nullptr, nullptr);
    
    if(numBytes < 0) {
      std::cerr << "Failed to receive data" << std::endl;
      break;
    }

    std::cout << "Received " << numBytes << " bytes" << std::endl;

    Ethernet::EthernetHeader ethHeader;
    if(Ethernet::parseEthernetHeader(buffer, numBytes, ethHeader)) {
      std::cout << "Ethernet Header:" << std::endl;
      std::cout << "Destination MAC: " << Ethernet::getMacAddressString(ethHeader.destination) << std::endl;
      std::cout << "Source MAC: " << Ethernet::getMacAddressString(ethHeader.source) << std::endl;
      std::cout << "Ethertype: 0x" << std::hex << ethHeader.ethertype << std::dec << std::endl;

      if(ethHeader.ethertype == static_cast<uint16_t>(Net::Ethernet::Type::ARP)) {
        std::cout << "Received ARP packet" << std::endl;
      } else {
        std::cout << "Received non-ARP packet" << std::endl;
      }
    } else {
      std::cerr << "Failed to parse Ethernet header" << std::endl;
    }
  }
}