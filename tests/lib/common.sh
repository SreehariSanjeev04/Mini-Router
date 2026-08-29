# Shared helpers for the mini-router integration tests (sourced, not executed).
#
# Contract with the aggregate runner (tests/run-all.sh):
#   exit 0 = test passed
#   exit 1 = test failed (prints its own reason)
#   exit 2 = test skipped (prerequisite not available, e.g. no root)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
BIN="$BUILD_DIR/Router"

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_RESET=$'\033[0m'
    C_BOLD=$'\033[1m'
    C_GREEN=$'\033[32m'
    C_RED=$'\033[31m'
    C_YELLOW=$'\033[33m'
else
    C_RESET=''; C_BOLD=''; C_GREEN=''; C_RED=''; C_YELLOW=''
fi

log()  { printf '%s%s== %s%s\n' "$C_BOLD" "${C_RESET}" "$*" "$C_RESET" >&2; }
pass() { printf '%sPASS: %s%s\n' "$C_GREEN" "$*" "$C_RESET" >&2; }
fail() { printf '%sFAIL: %s%s\n' "$C_RED" "$*" "$C_RESET" >&2; exit 1; }
skip() { printf '%sSKIP: %s%s\n' "$C_YELLOW" "$*" "$C_RESET" >&2; exit 2; }

require_root() {
    [ "$(id -u)" -eq 0 ] || skip "requires root. Re-run with sudo."
}

require_tools() {
    command -v ip >/dev/null || fail "'ip' command not found"
    command -v python3 >/dev/null || fail "python3 not found"
    python3 -c 'import socket, fcntl, struct' 2>/dev/null || fail "python3 needs socket/fcntl/struct modules"
}

build_router() {
    log "building router (cmake, $BUILD_DIR)"
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
    cmake --build "$BUILD_DIR" --clean-first -j >/dev/null
    [ -x "$BIN" ] || fail "cmake did not produce $BIN"
}

# link_mac <namespace> <dev>  ->  lowercase colon-separated MAC, e.g. aa:bb:cc:dd:ee:ff
link_mac() {
    ip -n "$1" link show "$2" | grep -o 'link/ether [0-9a-f:]*' | cut -d' ' -f2
}

# arp_probe <namespace> <dev> <src_ip> <tgt_ip> <timeout_s>
# Injects a real ARP request ("who-has <tgt_ip>" from <src_ip>) via a raw
# AF_PACKET socket and listens for ARP replies to that request.
# Prints to stdout:
#   SENT: <src_ip> -> who-has <tgt_ip>
#   REPLIES:<n>
#     op=<op> eth_src=<mac> sender=<ip> sender_mac=<mac>   (one line per reply)
arp_probe() {
    ip netns exec "$1" python3 - "$2" "$3" "$4" "$5" <<'PY'
import socket, struct, fcntl, time, sys

dev, src_ip, tgt_ip, timeout = sys.argv[1:5]

SIOCGIFHWADDR = 0x8927
ETH_P_ALL = 0x0003
ARP_TYPE = 0x0806

def be16(v):
    return struct.pack('!H', v)

sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ALL))

mac_str = fcntl.ioctl(sock, SIOCGIFHWADDR, struct.pack('256s', dev.encode()[:15]))
local_mac = mac_str[18:24]

src_ip4 = socket.inet_aton(src_ip)
tgt_ip4 = socket.inet_aton(tgt_ip)

frame = b'\xff' * 6 + local_mac + be16(ARP_TYPE)
frame += struct.pack('!HHBBH', 1, 0x0800, 6, 4, 1)
frame += local_mac + src_ip4 + b'\x00' * 6 + tgt_ip4

sock.sendto(frame, (dev, 0))
print('SENT: %s -> who-has %s' % (src_ip, tgt_ip))

replies = []
deadline = time.time() + float(timeout)
while time.time() < deadline:
    try:
        sock.settimeout(deadline - time.time())
        pkt = sock.recv(2048)
    except socket.timeout:
        break
    if len(pkt) < 14 + 28 or pkt[12:14] != be16(ARP_TYPE):
        continue
    arp = pkt[14:]
    op = struct.unpack('!H', arp[6:8])[0]
    if op != 2:
        continue
    replies.append('op=%d eth_src=%s sender=%s sender_mac=%s' % (
        op,
        pkt[6:12].hex(':'),
        socket.inet_ntoa(arp[14:18]),
        arp[8:14].hex(':')))

print('REPLIES:%d' % len(replies))
for r in replies:
    print('  ' + r)
PY
}

# reply_count <probe_output>  ->  number of ARP replies, 0 when unparseable
reply_count() {
    local c
    c="$(echo "$1" | sed -n 's/^REPLIES:\([0-9]*\)$/\1/p' | head -1)"
    echo "${c:-0}"
}