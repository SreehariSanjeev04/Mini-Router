#!/usr/bin/env bash
#
# Builds a 3-network-node topology for mini-router testing:
#
#    host1 (10.99.1.2) ---- veth (10.99.1.1) [ router ] (10.99.2.1) ---- veth ---- host2 (10.99.2.2)
#
# The router lives in its own namespace with two interfaces, one per subnet.
# Host1 and host2 each get their own namespace and are configured to route
# through the router. Kernel IP forwarding is enabled inside the router
# namespace so packets can pass end-to-end while the mini-router also
# observes them on its raw sockets.
#
# Usage:
#   sudo ./setup-topo.sh               # create the topology
#   sudo ./setup-topo.sh start         # create + run the mini-router
#   sudo ./setup-topo.sh stop          # stop the mini-router
#   sudo ./setup-topo.sh teardown      # destroy the topology
#   sudo ./setup-topo.sh test          # ping host2 from host1 through the router
#   sudo ./setup-topo.sh shell <ns>    # open a shell in host1|host2|router

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
BIN="$BUILD_DIR/Router"
LOG_FILE="$BUILD_DIR/router-topo.log"

# --- config -------------------------------------------------------------
RNS=mini-router-ns
H1NS=mini-host1-ns
H2NS=mini-host2-ns

VETH_R1=rp1            # router side toward host1
VETH_H1=hp1            # host1 side toward router
VETH_R2=rp2            # router side toward host2
VETH_H2=hp2            # host2 side toward router

ROUTER_IP1=10.99.1.1
HOST1_IP=10.99.1.2
ROUTER_IP2=10.99.2.1
HOST2_IP=10.99.2.2
PREFIX=24

ROUTER_PID=""

# --- helpers ------------------------------------------------------------
log()  { printf '== %s\n' "$*" >&2; }
err()  { printf '!! %s\n' "$*" >&2; exit 1; }

require_root() {
    [ "$(id -u)" -eq 0 ] || err "requires root. Re-run with sudo."
}

# --- cleanup -------------------------------------------------------------
cleanup() {
    if [ -n "$ROUTER_PID" ] && kill -0 "$ROUTER_PID" 2>/dev/null; then
        kill "$ROUTER_PID" 2>/dev/null || true
        wait "$ROUTER_PID" 2>/dev/null || true
        ROUTER_PID=""
    fi
    ip link del "$VETH_R1" >/dev/null 2>&1 || true
    ip netns del "$H1NS" >/dev/null 2>&1 || true
    ip netns del "$H2NS" >/dev/null 2>&1 || true
    ip netns del "$RNS" >/dev/null 2>&1 || true
}

# --- topology creation ---------------------------------------------------
create_topology() {
    log "creating namespaces and veth pairs"
    ip netns add "$RNS"
    ip netns add "$H1NS"
    ip netns add "$H2NS"

    # host1 <-> router (subnet 10.99.1.0/24)
    ip link add "$VETH_R1" type veth peer name "$VETH_H1"
    ip link set "$VETH_R1" netns "$RNS"
    ip link set "$VETH_H1" netns "$H1NS"

    # router <-> host2 (subnet 10.99.2.0/24)
    ip link add "$VETH_R2" type veth peer name "$VETH_H2"
    ip link set "$VETH_R2" netns "$RNS"
    ip link set "$VETH_H2" netns "$H2NS"

    # router addresses
    ip -n "$RNS" addr add "$ROUTER_IP1/$PREFIX" dev "$VETH_R1"
    ip -n "$RNS" addr add "$ROUTER_IP2/$PREFIX" dev "$VETH_R2"

    # host addresses
    ip -n "$H1NS" addr add "$HOST1_IP/$PREFIX" dev "$VETH_H1"
    ip -n "$H2NS" addr add "$HOST2_IP/$PREFIX" dev "$VETH_H2"

    # defaults
    ip -n "$RNS" addr add 127.0.0.1/8 dev lo || true
    ip -n "$H1NS" addr add 127.0.0.1/8 dev lo || true
    ip -n "$H2NS" addr add 127.0.0.1/8 dev lo || true

    ip -n "$RNS" link set lo up
    ip -n "$H1NS" link set lo up
    ip -n "$H2NS" link set lo up
    ip -n "$RNS" link set "$VETH_R1" up
    ip -n "$RNS" link set "$VETH_R2" up
    ip -n "$H1NS" link set "$VETH_H1" up
    ip -n "$H2NS" link set "$VETH_H2" up

    # hosts reach each other through the router
    ip -n "$H1NS" route add default via "$ROUTER_IP1" dev "$VETH_H1"
    ip -n "$H2NS" route add default via "$ROUTER_IP2" dev "$VETH_H2"

    # Kernel IP forwarding is DISABLED inside the router namespace so that only
    # the mini-router forwards packets between host1 and host2. The kernel keeps
    # answering ARP for the router's own IPs (required for the hosts' default
    # gateways to resolve). To let the kernel do the routing instead, set
    # KERNEL_FORWARD=1.
    ip netns exec "$RNS" sysctl -w net.ipv4.ip_forward="${KERNEL_FORWARD:-0}" >/dev/null

    log "topology created"
    status
}

# --- status --------------------------------------------------------------
status() {
    echo "namespaces:"
    ip netns list | grep -E "$RNS|$H1NS|$H2NS" || true
    echo
    echo "router ($RNS):"
    ip -n "$RNS" addr show 2>/dev/null | grep -E 'inet |ether ' || true
    echo
    echo "host1 ($H1NS):"
    ip -n "$H1NS" addr show 2>/dev/null | grep -E 'inet |ether ' || true
    echo
    echo "host2 ($H2NS):"
    ip -n "$H2NS" addr show 2>/dev/null | grep -E 'inet |ether ' || true
}

# --- start the mini-router ------------------------------------------------
start_router() {
    log "rebuilding router (cmake, $BUILD_DIR)"
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
    cmake --build "$BUILD_DIR" -j >/dev/null
    [ -x "$BIN" ] || err "$BIN not built. cmake produced no executable."
    kill -0 "$ROUTER_PID" 2>/dev/null && { log "router already running (pid $ROUTER_PID)"; return; }
    : > "$LOG_FILE"
    ip netns exec "$RNS" "$BIN" >"$LOG_FILE" 2>&1 &
    ROUTER_PID=$!
    sleep 0.5
    kill -0 "$ROUTER_PID" 2>/dev/null || { cat "$LOG_FILE" >&2; err "router exited at startup"; }
    log "router running (pid $ROUTER_PID), log: $LOG_FILE"
}

stop_router() {
    if [ -n "$ROUTER_PID" ] && kill -0 "$ROUTER_PID" 2>/dev/null; then
        kill "$ROUTER_PID" 2>/dev/null || true
        wait "$ROUTER_PID" 2>/dev/null || true
        log "router stopped"
    else
        log "router not running"
    fi
    ROUTER_PID=""
}

# --- test connectivity -----------------------------------------------------
do_test() {
    start_router
    log "ping $HOST2_IP from $H1NS (through router)"
    ip netns exec "$H1NS" ping -c 3 "$HOST2_IP" || true
    log "ping $HOST1_IP from $H2NS (through router)"
    ip netns exec "$H2NS" ping -c 3 "$HOST1_IP" || true
    log "router log tail:"
    tail -10 "$LOG_FILE" >&2 || true
}

# --- main -------------------------------------------------------------------
require_root

case "${1:-status}" in
    start)
        ip netns list | grep -q "^$RNS " || create_topology || true
        start_router
        ;;
    stop)
        stop_router
        ;;
    status)
        status
        ;;
    test)
        do_test
        ;;
    shell)
        ns="${2:-host1}"
        case "$ns" in
            router) tgt="$RNS" ;;
            host1)  tgt="$H1NS" ;;
            host2)  tgt="$H2NS" ;;
            *) err "unknown ns '$ns' (use router|host1|host2)" ;;
        esac
        ip netns exec "$tgt" bash
        ;;
    teardown)
        cleanup
        log "topology torn down"
        ;;
    *)
        err "usage: $0 {start|stop|status|test|shell <ns>|teardown}"
        ;;
esac
