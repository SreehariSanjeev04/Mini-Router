#include <cstdio>
#include <cstring>
#include <ios>
#include <iostream>

#include <arpa/inet.h>
#include <iomanip>
#include <linux/if_ether.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>

#define BUFFER_SIZE 65536

#define IPv4 0x0800
#define ARP 0x0806
#define IPv6 0x86DD
#define VLAN 0x8100

const std::unordered_map<uint16_t, std::string> ETHERTYPE_MAP = {
    {0x0800, "IPv4"},
    {0x0806, "ARP"},
    {0x8035, "RARP"},
    {0x8100, "802.1Q VLAN Tagged"},
    {0x86DD, "IPv6"},
    {0x8808, "Ethernet Flow Control"},
    {0x8847, "MPLS Unicast"},
    {0x8848, "MPLS Multicast"},
    {0x8863, "PPPoE Discovery"},
    {0x8864, "PPPoE Session"},
    {0x88A8, "802.1ad Service VLAN (QinQ)"},
    {0x88CC, "LLDP"},
    {0x88E5, "MACsec (802.1AE)"},
    {0x88F7, "PTP (Precision Time Protocol)"}};

std::string getEtherTypeString(uint16_t ethertype)
{
  auto it = ETHERTYPE_MAP.find(ethertype);
  if (it != ETHERTYPE_MAP.end())
  {
    return it->second;
  }

  std::ostringstream ss;
  ss << "Unknown (0x"
     << std::hex << std::uppercase
     << std::setw(4) << std::setfill('0')
     << ethertype << ")";
  return ss.str();
}

void printMac(const unsigned char *mac)
{
  printf("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3],
         mac[4], mac[5]);
}

void print_ethernet_frame(struct ethhdr *ethernet_header)
{
  printf("Ethernet Frame\n");
  printf("-----------------------\n");
  printf("Source: ");
  printMac(ethernet_header->h_source);
  printf("\n");
  printf("Destination: ");
  printMac(ethernet_header->h_dest);
  printf("\n");
  uint16_t ethertype_host = ntohs(ethernet_header->h_proto);
  printf("EtherType: %s\n", getEtherTypeString(ethertype_host).c_str());
  printf("\n\n");
}

int main()
{
  int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sockfd < 0)
  {
    perror("socket");
    return 0;
  }

  std::cout << "Raw socket created\n";

  u_int8_t network_rx_buffer[BUFFER_SIZE];

  while (true)
  {
    ssize_t size = recvfrom(sockfd, network_rx_buffer,
                            sizeof(network_rx_buffer), 0, nullptr, nullptr);
    if (size < 0)
    {
      perror("recvfrom");
      return 1;
    }

    if (size < sizeof(struct ethhdr))
    {
      std::cout << "Packet too small\n";
      continue;
    }

    std::cout << "Received packet: " << size << " bytes\n";

    struct ethhdr *received_eth = (struct ethhdr *)network_rx_buffer;
    print_ethernet_frame(received_eth);
  }
  close(sockfd);

  return 0;
}
