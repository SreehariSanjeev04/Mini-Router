#!/usr/bin/env bash
#
# Proxy-ARP end-to-end test.
#
# Topology:
#   router ns (mini-px-rns, 10.99.0.1 on veth-pxr, 10.99.1.1 on veth-pxr2)
#     |---- veth-pxr / veth-pxh ---- host ns   (mini-px-hns , 10.99.0.2)
#     |---- veth-pxr2/veth-pxh2 ---- client ns (mini-px-nsc , 10.99.1.2)
#
#   Step 1: the host asks "who-has 10.99.0.1" -> router answers and learns the
#           host's IP/MAC into its ARP cache.
#   Step 2: the client asks "who-has 10.99.0.2" (the host's IP, which is NOT a
#           router interface). The router must answer on the client's link via
#           proxy ARP, using the MAC it cached for 10.99.0.2. The in-namespace
#           kernel does not answer here (10.99.0.2 is not local to it), so a
#           working router produces exactly 1 reply.

set -euo pipefail

TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/common.sh
source "$TESTS_DIR/lib/common.sh"

RNS=mini-px-rns
HNS=mini-px-hns
CNS=mini-px-nsc
VETH_R=veth-pxr
VETH_H=veth-pxh
VETH_R2=veth-pxr2
VETH_H2=veth-pxh2
ROUTER_IP=10.99.0.1
ROUTER_IP2=10.99.1.1
PEER_IP=10.99.0.2
REMOTE_IP=10.99.1.2
PREFIX=24
LOG_FILE="$BUILD_DIR/test-proxy-arp.log"

ROUTER_PID=""
topology_cleanup() {
    if [ -n "$ROUTER_PID" ] && kill -0 "$ROUTER_PID" 2>/dev/null; then
        kill "$ROUTER_PID" 2>/dev/null || true
        wait "$ROUTER_PID" 2>/dev/null || true
    fi
    ip link del "$VETH_R" >/dev/null 2>&1 || true
    ip link del "$VETH_R2" >/dev/null 2>&1 || true
    ip netns del "$RNS" >/dev/null 2>&1 || true
    ip netns del "$HNS" >/dev/null 2>&1 || true
    ip netns del "$CNS" >/dev/null 2>&1 || true
}

require_root
require_tools
build_router

trap topology_cleanup EXIT
topology_cleanup

log "setting up topology: $HNS($PEER_IP) <-veth> $RNS($ROUTER_IP) <-veth2> $CNS($REMOTE_IP)"
ip netns add "$RNS"
ip netns add "$HNS"
ip netns add "$CNS"
ip link add "$VETH_R" type veth peer name "$VETH_H"
ip link add "$VETH_R2" type veth peer name "$VETH_H2"
ip link set "$VETH_R" netns "$RNS"
ip link set "$VETH_H" netns "$HNS"
ip link set "$VETH_R2" netns "$RNS"
ip link set "$VETH_H2" netns "$CNS"
ip -n "$RNS" addr add "$ROUTER_IP/$PREFIX" dev "$VETH_R"
ip -n "$RNS" addr add "$ROUTER_IP2/$PREFIX" dev "$VETH_R2"
ip -n "$HNS" addr add "$PEER_IP/$PREFIX" dev "$VETH_H"
ip -n "$CNS" addr add "$REMOTE_IP/$PREFIX" dev "$VETH_H2"
ip -n "$RNS" link set lo up
ip -n "$RNS" link set "$VETH_R" up
ip -n "$RNS" link set "$VETH_R2" up
ip -n "$HNS" link set lo up
ip -n "$HNS" link set "$VETH_H" up
ip -n "$CNS" link set lo up
ip -n "$CNS" link set "$VETH_H2" up

log "starting router in namespace '$RNS' (log: $LOG_FILE)"
: > "$LOG_FILE"
ip netns exec "$RNS" "$BIN" >"$LOG_FILE" 2>&1 &
ROUTER_PID=$!
sleep 0.5
kill -0 "$ROUTER_PID" 2>/dev/null || { cat "$LOG_FILE" >&2; fail "router exited at startup"; }
grep -q 'Local Interfaces' "$LOG_FILE" || { cat "$LOG_FILE" >&2; fail "router did not list local interfaces at startup"; }
pass "router running with pid $ROUTER_PID"

VETH_H_MAC="$(link_mac "$HNS" "$VETH_H")"

log "step 1: $HNS asks who-has $ROUTER_IP (router learns $PEER_IP -> $VETH_H_MAC)"
probe1="$(arp_probe "$HNS" "$VETH_H" "$PEER_IP" "$ROUTER_IP" 3 2>&1 || true)"
echo "$probe1" >&2
count1="$(reply_count "$probe1")"
grep -q 'Failed to send ARP reply' "$LOG_FILE" && { cat "$LOG_FILE" >&2; fail "router logged a send failure in step 1"; }
[ "$count1" -ge 2 ] || fail "step 1: expected >= 2 replies (router + kernel), got $count1; router did not answer/learn"

log "step 2: $CNS asks who-has $PEER_IP (expect proxy ARP reply from router)"
probe2="$(arp_probe "$CNS" "$VETH_H2" "$REMOTE_IP" "$PEER_IP" 3 2>&1 || true)"
echo "$probe2" >&2
count2="$(reply_count "$probe2")"
grep -q 'Failed to send ARP reply' "$LOG_FILE" && { cat "$LOG_FILE" >&2; fail "router logged a send failure in step 2"; }

[ "$count2" -eq 1 ] || {
    [ "$count2" -eq 0 ] && fail "no proxy ARP reply received; cache lookup failed or request never reached the router"
    fail "step 2: expected exactly 1 proxy reply, got $count2"
}

line="$(echo "$probe2" | grep -m1 '^  op=2 ')"
[[ "$line" == *"sender=$PEER_IP"* ]]  || fail "proxy reply sender IP is not $PEER_IP: $line"
[[ "$line" == *"sender_mac=$VETH_H_MAC"* ]] || fail "proxy reply sender MAC is not $VETH_H_MAC: $line"
[[ "$line" == *"eth_src=$VETH_H_MAC"* ]] || fail "proxy reply ethernet source is not $VETH_H_MAC: $line"

grep -q "Sent ARP reply to $ROUTER_IP" "$LOG_FILE" || fail "router did not log the step-1 reply to $ROUTER_IP"
grep -Fq "Sent ARP reply to $REMOTE_IP" "$LOG_FILE" || { cat "$LOG_FILE" >&2; fail "router did not log the proxy reply to $REMOTE_IP"; }

log "router log tail:"
tail -8 "$LOG_FILE" >&2
pass "proxy ARP reply delivered on the client link: $PEER_IP -> $VETH_H_MAC"
exit 0