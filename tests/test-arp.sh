#!/usr/bin/env bash
#
# Basic ARP end-to-end test:
#   router (10.99.0.1) in an isolated namespace, host (10.99.0.2) on the other
#   end of a veth pair. The host asks "who-has 10.99.0.1" and must receive
#   well-formed ARP replies naming the router. The in-namespace kernel also
#   answers for its own IP, so a working router yields >= 2 replies.

set -euo pipefail

TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/common.sh
source "$TESTS_DIR/lib/common.sh"

RNS=mini-arp-rns
HNS=mini-arp-hns
VETH_R=veth-ar
VETH_H=veth-ah
ROUTER_IP=10.99.0.1
PEER_IP=10.99.0.2
PREFIX=24
LOG_FILE="$BUILD_DIR/test-arp.log"

ROUTER_PID=""
topology_cleanup() {
    if [ -n "$ROUTER_PID" ] && kill -0 "$ROUTER_PID" 2>/dev/null; then
        kill "$ROUTER_PID" 2>/dev/null || true
        wait "$ROUTER_PID" 2>/dev/null || true
    fi
    ip link del "$VETH_R" >/dev/null 2>&1 || true
    ip netns del "$RNS" >/dev/null 2>&1 || true
    ip netns del "$HNS" >/dev/null 2>&1 || true
}

require_root
require_tools
build_router

trap topology_cleanup EXIT
topology_cleanup

log "setting up topology: $RNS <-> $HNS ($ROUTER_IP/$PREFIX)"
ip netns add "$RNS"
ip netns add "$HNS"
ip link add "$VETH_R" type veth peer name "$VETH_H"
ip link set "$VETH_R" netns "$RNS"
ip link set "$VETH_H" netns "$HNS"
ip -n "$RNS" addr add "$ROUTER_IP/$PREFIX" dev "$VETH_R"
ip -n "$HNS" addr add "$PEER_IP/$PREFIX" dev "$VETH_H"
ip -n "$RNS" link set lo up
ip -n "$RNS" link set "$VETH_R" up
ip -n "$HNS" link set lo up
ip -n "$HNS" link set "$VETH_H" up

log "starting router in namespace '$RNS' (log: $LOG_FILE)"
: > "$LOG_FILE"
ip netns exec "$RNS" "$BIN" >"$LOG_FILE" 2>&1 &
ROUTER_PID=$!
sleep 0.5
kill -0 "$ROUTER_PID" 2>/dev/null || { cat "$LOG_FILE" >&2; fail "router exited at startup"; }
grep -q 'Local Interfaces' "$LOG_FILE" || { cat "$LOG_FILE" >&2; fail "router did not list local interfaces at startup"; }
pass "router running with pid $ROUTER_PID"

VETH_R_MAC="$(link_mac "$RNS" "$VETH_R")"

log "ARP request: $HNS asks who-has $ROUTER_IP"
probe_out="$(arp_probe "$HNS" "$VETH_H" "$PEER_IP" "$ROUTER_IP" 3 2>&1 || true)"
echo "$probe_out" >&2
count="$(reply_count "$probe_out")"

grep -q 'Failed to send ARP reply' "$LOG_FILE" && { cat "$LOG_FILE" >&2; fail "router logged a send failure"; }
grep -q 'Sent ARP reply to' "$LOG_FILE" || { cat "$LOG_FILE" >&2; fail "router did not log sending a reply"; }

[ "$count" -ge 2 ] || {
    [ "$count" -eq 1 ] && fail "only the in-namespace kernel answered; the router did not (raise route: interface enumeration via SIOCGIFCONF, then handleARPPacket)"
    fail "no ARP reply received for $ROUTER_IP (got $count)"
}

# every reply must come from the router side and name the router's IP + MAC
bad="$(echo "$probe_out" | grep '^  ' | grep -v "sender=$ROUTER_IP" || true)"
[ -z "$bad" ] || fail "reply sender IP mismatch: $bad"
bad="$(echo "$probe_out" | grep '^  ' | grep -v "sender_mac=$VETH_R_MAC" || true)"
[ -z "$bad" ] || fail "reply sender MAC mismatch (want $VETH_R_MAC): $bad"
bad="$(echo "$probe_out" | grep '^  ' | grep -v "eth_src=$VETH_R_MAC" || true)"
[ -z "$bad" ] || fail "reply ethernet source mismatch (want $VETH_R_MAC): $bad"

log "router log tail:"
tail -6 "$LOG_FILE" >&2
pass "$count correct ARP replies for $ROUTER_IP (router + kernel)"
exit 0