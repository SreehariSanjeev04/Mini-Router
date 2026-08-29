#!/usr/bin/env bash
#
# Test the mini-router's ARP handling end-to-end.
#
# Usage:
#   scripts/test-arp.sh unit     # offline protocol checks (no root required)
#   scripts/test-arp.sh e2e      # full loopback test through a raw socket (root required)
#   scripts/test-arp.sh          # auto: e2e if root, otherwise unit
#
# e2e mode:
#   Builds the router and runs it inside an isolated network namespace, while a
#   peer namespace sends a real ARP request over a veth pair. The test then
#   verifies that a well-formed ARP reply for the router's address arrives.
#   Requires root (CAP_NET_ADMIN for namespaces, CAP_NET_RAW for raw sockets).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
BIN="$BUILD_DIR/Router"
MODE="${1:-auto}"

# e2e topology
RNS=mini-router-rns
HNS=mini-router-hns
VETH_R=veth-r
VETH_H=veth-h
ROUTER_IP=10.99.0.1
PEER_IP=10.99.0.2
PREFIX=24
LOG_FILE="$BUILD_DIR/router.log"

log()  { printf '== %s\n' "$*" >&2; }
fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
pass() { printf 'PASS: %s\n' "$*" >&2; }

build() {
    log "building router (cmake, $BUILD_DIR)"
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
    cmake --build "$BUILD_DIR" --clean-first -j >/dev/null
    [ -x "$BIN" ] || fail "build did not produce $BIN"
}

cleanup_e2e() {
    if [ -n "${ROUTER_PID:-}" ] && kill -0 "$ROUTER_PID" 2>/dev/null; then
        kill "$ROUTER_PID" 2>/dev/null || true
        wait "$ROUTER_PID" 2>/dev/null || true
    fi
    ip link del "$VETH_R" >/dev/null 2>&1 || true
    ip netns del "$RNS" >/dev/null 2>&1 || true
    ip netns del "$HNS" >/dev/null 2>&1 || true
}

run_unit() {
    local harness=/tmp/arp-offline
    log "unit: compiling offline ARP harness"
    g++ -std=gnu++17 -I "$ROOT_DIR/src" -Wall -Wextra -Wpedantic -o "$harness" \
        "$ROOT_DIR/tests/arp_offline.cpp" \
        "$ROOT_DIR/src/arp/ARP.cpp" \
        "$ROOT_DIR/src/arp/ArpCache.cpp" \
        "$ROOT_DIR/src/arp/ArpHandler.cpp" \
        "$ROOT_DIR/src/interface/InterfaceManager.cpp" \
        "$ROOT_DIR/src/network/RawSocket.cpp" \
        "$ROOT_DIR/src/packet/ethernet/Ethernet.cpp"
    log "unit: running offline ARP harness"
    "$harness"
}

run_e2e() {
    [ "$(id -u)" -eq 0 ] || fail "e2e mode requires root. Use 'unit' mode without root."
    command -v ip >/dev/null || fail "'ip' command not found"
    python3 -c 'import socket, fcntl, struct' 2>/dev/null || fail "python3 with socket/fcntl/struct required"

    build
    trap cleanup_e2e EXIT
    cleanup_e2e

    log "e2e: creating namespaces and veth pair"
    ip netns add "$RNS"
    ip netns add "$HNS"
    ip link add "$VETH_R" type veth peer name "$VETH_H"
    ip link set "$VETH_R" netns "$RNS"
    ip link set "$VETH_H" netns "$HNS"
    ip -n "$RNS" addr add "$ROUTER_IP/$PREFIX" dev "$VETH_R"
    ip -n "$HNS" addr add "$PEER_IP/$PREFIX" dev "$VETH_H"
    ip -n "$RNS" link set lo up
    ip -n "$RNS" link set "$VETH_R" up
    ip -n "$HNS" link set "$VETH_H" up

    log "e2e: starting router in namespace '$RNS' (log: $LOG_FILE)"
    : > "$LOG_FILE"
    ip netns exec "$RNS" "$BIN" >"$LOG_FILE" 2>&1 &
    ROUTER_PID=$!
    sleep 0.5
    kill -0 "$ROUTER_PID" 2>/dev/null || {
        cat "$LOG_FILE" >&2
        fail "router exited at startup"
    }
    grep -q 'Local Interfaces' "$LOG_FILE" || {
        cat "$LOG_FILE" >&2
        fail "router did not list local interfaces at startup"
    }
    pass "router running with pid $ROUTER_PID"

    log "e2e: sending ARP request for $ROUTER_IP from peer namespace '$HNS'"
    local probe_out
    if ! probe_out="$(ip netns exec "$HNS" python3 - "$VETH_H" "$PEER_IP" "$ROUTER_IP" 2>&1 <<'PY' || true
import socket, struct, fcntl, time, sys

dev, src_ip, tgt_ip = sys.argv[1:4]

SIOCGIFHWADDR = 0x8927
ETH_P_ALL = 0x0003
ARP_TYPE = 0x0806

def mac(obj):
    return obj[:6]

# 2-byte big-endian encoding for on-the-wire fields (frame bytes)
def be16(v):
    return struct.pack('!H', v)

# socket() protocol argument must be an int, in network byte order
sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ALL))
# Intentionally NOT bound: an ETH_P_ALL socket receives frames on all
# interfaces. Binding via Python's sockaddr_ll applies htons() to the protocol
# field (double-swap trap), so we avoid it entirely.

# local interface MAC via SIOCGIFHWADDR ioctl
mac_str = fcntl.ioctl(sock, SIOCGIFHWADDR, struct.pack('256s', dev.encode()[:15]))
local_mac = mac_str[18:24]

src_ip4 = socket.inet_aton(src_ip)
tgt_ip4 = socket.inet_aton(tgt_ip)

# Ethernet header: broadcast dst, local src, ARP ethertype
frame = b'\xff' * 6 + local_mac + be16(ARP_TYPE)
# ARP request: htype, ptype, hlen, plen, opcode, sha, sip, tha, tip
frame += struct.pack('!HHBBH', 1, 0x0800, 6, 4, 1)
frame += local_mac + src_ip4 + b'\x00' * 6 + tgt_ip4

sock.sendto(frame, (dev, 0))

replies = []
deadline = time.time() + 3.0
while time.time() < deadline:
    try:
        sock.settimeout(deadline - time.time())
        pkt = sock.recv(2048)
    except socket.timeout:
        break
    if len(pkt) < 14 + 28:
        continue
    if pkt[12:14] != be16(ARP_TYPE):
        continue
    arp = pkt[14:]
    op = struct.unpack('!H', arp[6:8])[0]
    if op != 2:
        continue
    mac_sender = mac(arp[8:14])
    ip_sender = socket.inet_ntoa(arp[14:18])
    if ip_sender != tgt_ip:
        continue
    eth_src = mac(pkt[6:12])
    replies.append((op, eth_src.hex(':'), ip_sender, mac_sender.hex(':')))

print('REPLIES:%d' % len(replies))
for r in replies:
    print('  op=%d eth_src=%s sender=%s sender_mac=%s' % r)
PY
)"; then
        true
    fi

    log "router log tail:"
    tail -8 "$LOG_FILE" >&2

    echo "$probe_out" >&2
    local replies
    replies="$(echo "$probe_out" | sed -n 's/^REPLIES:\([0-9]*\)$/\1/p' | head -1 || true)"
    [ -n "$replies" ] || { cat "$LOG_FILE" >&2; fail "probe produced no reply summary"; }

    grep -q 'Failed to send ARP reply' "$LOG_FILE" && {
        cat "$LOG_FILE" >&2
        fail "router failed to send the ARP reply"
    }

    if [ "$replies" -ge 2 ]; then
        pass "received $replies correct ARP replies (router + kernel) for $ROUTER_IP"
    elif [ "$replies" -eq 1 ]; then
        fail "only the in-namespace kernel answered; the router did not reply (check interface enumeration via SIOCGIFCONF, then the router handleARPPacket path)"
    else
        fail "no ARP reply received for $ROUTER_IP"
    fi
}

case "$MODE" in
    unit) run_unit ;;
    e2e)  run_e2e ;;
    auto)
        if [ "$(id -u)" -eq 0 ]; then
            log "root detected -> e2e mode (use 'unit' for offline checks)"
            run_e2e
        else
            log "no root -> unit mode (use 'e2e' for the full loopback test)"
            run_unit
        fi
        ;;
    *) echo "usage: $0 [unit|e2e]" >&2; exit 2 ;;
esac