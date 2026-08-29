#include <arpa/inet.h>
#include <net/ethernet.h>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "arp/ARP.h"
#include "arp/ArpCache.h"
#include "arp/ArpHandler.h"
#include "commons/Constants.h"
#include "interface/InterfaceManager.h"
#include "network/RawSocket.h"
#include "packet/ethernet/Ethernet.h"

namespace {

int failures = 0;

void check(bool cond, const char* what)
{
    std::cout << (cond ? "[PASS] " : "[FAIL] ") << what << "\n";
    if (!cond)
    {
        ++failures;
    }
}

const std::array<uint8_t, 6> kRouterMac = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
const std::array<uint8_t, 4> kRouterIp  = {10, 99, 0, 1};
const std::array<uint8_t, 6> kPeerMac   = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
const std::array<uint8_t, 4> kPeerIp    = {10, 99, 0, 2};

void putMac(uint8_t* out, const std::array<uint8_t, 6>& mac) { std::memcpy(out, mac.data(), mac.size()); }
void putIp(uint8_t* out, const std::array<uint8_t, 4>& ip) { std::memcpy(out, ip.data(), ip.size()); }
void put16(uint8_t* out, uint16_t v) { uint16_t w = htons(v); std::memcpy(out, &w, 2); }

size_t buildArpRequestFrame(uint8_t* frame)
{
    size_t offset = 0;


    Ethernet::EthernetHeader eth;
    eth.ethertype = static_cast<uint16_t>(Net::Ethernet::Type::ARP);
    for (uint8_t& b : eth.destination.bytes) b = 0xff; // broadcast
    putMac(eth.source.bytes, kPeerMac);
    Ethernet::serialize(eth, frame + offset, sizeof(Ethernet::EthernetHeader));
    offset += sizeof(Ethernet::EthernetHeader);

    // ARP request
    put16(frame + offset, Net::ARP::HARDWARE_ETHERNET); offset += 2;
    put16(frame + offset, static_cast<uint16_t>(Net::Ethernet::Type::IPv4)); offset += 2;
    frame[offset++] = Net::ARP::HARDWARE_LENGTH;
    frame[offset++] = Net::ARP::PROTOCOL_LENGTH;
    put16(frame + offset, static_cast<uint16_t>(Net::ARP::Opcode::Request)); offset += 2;
    putMac(frame + offset, kPeerMac); offset += 6;
    putIp(frame + offset, kPeerIp); offset += 4;
    for (size_t i = 0; i < 6; ++i) frame[offset++] = 0; 
    putIp(frame + offset, kRouterIp); offset += 4;

    return offset;
}

} // namespace

int main()
{
    RawSocket socket(ETH_P_ALL); 
    InterfaceManager interfaces;
    interfaces.addLocalInterface(kRouterMac, kRouterIp);

    ArpCache cache;
    ArpHandler handler(cache, interfaces, socket);

    uint8_t frame[Net::BUFFER_SIZE];
    size_t frameLen = buildArpRequestFrame(frame);

    Ethernet::EthernetHeader reqEth;
    check(Ethernet::parseEthernetHeader(frame, frameLen, reqEth), "request ethernet header parses");
    check(reqEth.ethertype == static_cast<uint16_t>(Net::Ethernet::Type::ARP), "request ethertype is ARP");

    ARP::Message req;
    check(ARP::parse(frame + sizeof(Ethernet::EthernetHeader),
                     frameLen - sizeof(Ethernet::EthernetHeader), req),
          "request ARP payload parses");
    check(ARP::isRequest(req), "request opcode is request");
    check(req.target_ip == kRouterIp, "request targets the router ip");

    uint8_t response[Net::BUFFER_SIZE];
    bool replied = handler.handleARPPacket(frame, frameLen, response, sizeof(response));

    // A reply frame must always be fully built; it is only the final send() that
    // can fail without CAP_NET_RAW, turning the return value false.
    std::cout << "handleARPPacket returned " << (replied ? "true" : "false")
              << " (false expected without CAP_NET_RAW)\n";

    Ethernet::EthernetHeader replyEth;
    check(Ethernet::parseEthernetHeader(response, frameLen, replyEth), "reply ethernet header parses");
    check(std::memcmp(replyEth.destination.bytes, kPeerMac.data(), 6) == 0,
          "reply ethernet dst == requester mac");
    check(std::memcmp(replyEth.source.bytes, kRouterMac.data(), 6) == 0,
          "reply ethernet src == router mac");
    check(replyEth.ethertype == static_cast<uint16_t>(Net::Ethernet::Type::ARP),
          "reply ethertype is ARP");

    ARP::Message reply;
    check(ARP::parse(response + sizeof(Ethernet::EthernetHeader),
                     frameLen - sizeof(Ethernet::EthernetHeader), reply),
          "reply ARP payload parses");
    check(ARP::isReply(reply), "reply opcode is reply");
    check(reply.sender_mac == kRouterMac, "reply sender_mac == router mac");
    check(reply.sender_ip == kRouterIp, "reply sender_ip == router ip");
    check(reply.target_mac == kPeerMac, "reply target_mac == requester mac");
    check(reply.target_ip == kPeerIp, "reply target_ip == requester ip");

    check(cache.contains(kPeerIp), "cache contains requester ip");
    auto learnedMac = cache.get(kPeerIp);
    check(learnedMac.has_value() && *learnedMac == kPeerMac, "cache learned requester mac");

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures == 0 ? 0 : 1;
}