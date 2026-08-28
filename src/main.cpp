#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 65536

#define IPv4 0x0800
#define ARP 0x0806
#define IPv6 0x86DD
#define VLAN 0x8100

void printMac(const unsigned char *mac) {
  printf("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3],
         mac[4], mac[5]);
}

void print_ethernet_frame(struct ethhdr *ethernet_header) {
  printf("Ethernet Frame\n");
  printf("-----------------------\n");
  printf("Source: ");
  printMac(ethernet_header->h_source);
  printf("\n");
  printf("Destination: ");
  printMac(ethernet_header->h_dest);
  printf("\n\n");
}

int main() {
  int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sockfd < 0) {
    perror("socket");
    return 0;
  }

  std::cout << "Raw socket created\n";

  u_int8_t network_rx_buffer[BUFFER_SIZE];

  while (true) {
    ssize_t size = recvfrom(sockfd, network_rx_buffer,
                            sizeof(network_rx_buffer), 0, nullptr, nullptr);
    if (size < 0) {
      perror("recvfrom");
      return 1;
    }

    if (size < sizeof(struct ethhdr)) {
      std::cout << "Packet tool small\n";
      continue;
    }

    std::cout << "Received packet: " << size << " bytes\n";

    struct ethhdr *received_eth = (struct ethhdr *)network_rx_buffer;
    print_ethernet_frame(received_eth);
  }
  close(sockfd);

  return 0;
}
