# Mini-Router

A user-space IPv4 router implemented from scratch in C++17. Mini-Router reads
raw Ethernet frames straight off the NIC (`AF_PACKET` raw sockets), parses
Ethernet, ARP, and IPv4 headers byte-for-byte, and performs real IP forwarding
between directly connected subnets — complete with TTL decrementing, header
checksum recomputation, automatic ARP resolution, and ICMP echo handling.

No packet-parsing library, no kernel routing: Mini-Router does the work itself.

## Features

- **Raw L2 I/O** — per-interface `AF_PACKET`/`SOCK_RAW` sockets bound to each
  local port, multiplexed through `poll`.
- **Ethernet parsing & serialization** — full 14-byte header handling, MAC/Wire
  byte-order conversion.
- **ARP** — request/reply parsing, serialization, ARP-cache learning from any
  observed ARP sender, and most importantly **directed ARP requests** to
  resolve next-hop MACs on demand.
- **IPv4 parsing** — bit-level extraction of the version/IHL and flags/fragment
  fields, validation of total length vs. header size vs. buffer length, and
  RFC 1071 header-checksum verification.
- **IP forwarding** — longest-prefix-match routing over directly connected
  subnets, TTL decrement, IPv4 header-checksum refresh, and L2 re-addressing
  (src = outbound-interface MAC, dst = next-hop MAC).
- **ARP resolution on demand** — when a next-hop MAC is unknown, the router
  queues the packet (bounded queue) and issues a directed ARP request; queued
  packets are flushed as soon as the mapping is learned.
- **ICMP echo (local delivery)** — answers `ping` to the router's own IPs with
  a checksum-correct echo reply built from scratch.

## Architecture

```
                            ┌────────────────────────────┐
                  host1     │       Router (userspace)    │      host2
             10.99.1.2      │                             │  10.99.2.2
   ┌──────────────────┐     │  ┌────────────────────────┐ │  ┌──────────────────┐
   │ default via 1.1  │◄────►│  │ poll loop → recv frame │ │  │ default via 2.1  │
   └──────────────────┘ veth │  │  └─────┬───────────┘   │ └──────────────────┘
                             │  ┌───────▼───────────────┐│
                             │  │ parse Ethernet header  ││
                             │  └───┬────────────┬───────┘│
                             │   ARP│           IPv4│     │
                             │   ┌──▼──┐      ┌────▼─────┐│
                             │   │ ARP │      │ parse    ││
                             │   │hand.│      │validate  ││
                             │   └─────┘      └────┬─────┘│
                             │         dest = us? │       │
                             │        ┌───────────┤       │
                             │        │no         │yes    │
                             │   ┌────▼────┐   ┌───▼─────┐│
                             │   │ forward │   │  ICMP   ││
                             │   │  (LPM)  │   │  echo   ││
                             │   └────┬────┘   │  reply  ││
                             │   TTL--,checksum│  (local)││
                             │   ARP resolve →│  ────────┘│
                             │   new L2 header└───────────│┐
                             └────────────────────────────┘
```

### Receive loop flow

For every frame arriving on any interface:

1. Parse the Ethernet header and dispatch on the EtherType.
2. **ARP** → learn the sender `ip → mac` mapping, answer requests aimed at a
   router interface, then flush any packets queued waiting on the learned entry.
3. **IPv4** → parse and verify the header checksum (drop on failure).
   - Destination is a **router IP** → local delivery (ICMP echo reply).
   - Otherwise → forward: longest-prefix-match route lookup, decrement TTL,
     recompute header checksum, resolve next-hop MAC (issuing a directed ARP
     request and queueing the packet if needed), rebuild the Ethernet header,
     send on the outbound interface.

## Project layout

```
src/
├── arp/
│   ├── ARP.{h,cpp}            # ARP wire format: parse/serialize/createReply
│   ├── ArpCache.{h,cpp}       # IP -> MAC learning cache
│   └── ArpHandler.{h,cpp}     # answers requests + sends directed requests
├── commons/
│   └── Constants.h            # EtherTypes, ARP constants, IPv4 protocols
├── interface/
│   └── InterfaceManager.{h,cpp}  # interface discovery (ioctl), MAC/IP/mask
├── network/
│   └── RawSocket.{h,cpp}      # AF_PACKET raw socket bound to one interface
├── packet/
│   ├── ethernet/Ethernet.{h,cpp}
│   ├── icmp/ICMP.{h,cpp}      # echo request parsing + reply construction
│   └── ipv4/IPv4.{h,cpp}      # IPv4 header parse/serialize + checksums
│   └── ipv4/IPv4Packet.h      # parsed IPv4 representation
│   └── ipv4/IPv4Address.h     # 4-byte address type
├── router/
│   └── Router.{h,cpp}         # composition root + poll/receive loop
└── routing/
    ├── RoutingTable.{h,cpp}   # longest-prefix-match routing table
    └── IpForwarder.{h,cpp}    # forwarding engine + pending ARP queue
```

## Building and running

Requirements:

- Linux with `iproute2` (`ip`), `g++` with C++17 support, `cmake >= 3.10`
- `root` / `CAP_NET_RAW` — raw sockets require privilege.

```bash
cmake -S . -B build
cmake --build build
```

This produces `build/Router`.

### Testing topology

`setup-topo.sh` builds a three-namespace topology to exercise the router:

```
host1 (10.99.1.2) --- veth --- [ router (10.99.1.1 | 10.99.2.1) ] --- veth --- host2 (10.99.2.2)
```

```bash
sudo ./setup-topo.sh start       # create namespaces (if needed) and launch the router
sudo ./setup-topo.sh test        # ping host2 from host1 and vice versa
sudo ./setup-topo.sh shell host1 # drop into a host/namespace shell
sudo ./setup-topo.sh status      # show interfaces/addresses
sudo ./setup-topo.sh stop        # stop the router
sudo ./setup-topo.sh teardown    # destroy the topology
```

The router log is written to `build/router-topo.log`. Kernel IP forwarding is
**disabled** inside the router namespace so packets genuinely flow through the
Mini-Router; set `KERNEL_FORWARD=1` to let the kernel do the routing instead.

> Note: the in-namespace kernel still answers ARP for the router's own IPs
> (the hosts' default gateways need them to resolve). This is expected.

### End-to-end behavior

A single `ping 10.99.2.2` from `host1` demonstrates the full pipeline:

1. host1 ARPs for its gateway (`10.99.1.1`); the router learns `host1`'s MAC
   and replies.
2. host1 sends the ICMP echo frame to the router.
3. The router parses IPv4, verifies the checksum, finds the connected route via
   `10.99.2.0/24`, decrements the TTL, and recomputes the header checksum.
4. The next-hop MAC (`host2`) is unknown → the router queues the packet, sends
   a directed ARP request on `rp2`, and forwards once `host2` replies.
5. host2 receives a valid IPv4 packet sourced from `host1` and responds; its
   reply is forwarded back using the already-cached `host1` MAC.

## Automated tests

`tests/test-arp.sh` validates the ARP path end-to-end (an isolated namespace,
a veth pair, and a probe that asserts well-formed ARP replies naming the
router's IP and MAC).

```bash
sudo bash tests/test-arp.sh
```

## Design notes

- **Byte-order discipline** — every multi-byte on-wire field goes through
  `htons`/`ntohs`; parsing uses `memcpy` from raw bytes (never casting), which
  avoids unaligned-access and strict-aliasing issues.
- **Helpers, not guesses** — `IPv4::calculateChecksum`, `verifyChecksum`
  (checksum over a valid full header folds to `0x0000`), and `updateChecksum`
  for TTL-induced recomputation.
- **Owned payloads** — parsed packets reference the RX buffer, so the forwarder
  always copies/serializes bytes into its own storage before queueing.
- **Bounded pending queue** — ARP-resolution wait states are capped per target
  to prevent unbounded memory growth.

## Limitations / future work

- No ARP cache ageing or retry timers (a directed request is sent once).
- No fragmented-packet reassembly or fragment handling beyond header parsing.
- No ICMP error generation (TTL exceeded, destination unreachable).
- No static/learned gateway routes — only directly connected subnets are in the
  routing table.
- No IPv6, VLAN, or multicast support.

## License

Provided as-is for educational use.