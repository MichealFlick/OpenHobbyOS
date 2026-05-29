#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/ports/lwip"}
SYSROOT=${2:-"$ROOT/build/ports/sysroot"}
TARGET=${TARGET:-i686-openhobbyos}

LWIP_SRC="$ROOT/user/lib/lwip/src"
PORT_DIR="$ROOT/ports/lwip"

mkdir -p "$BUILD_DIR" "$SYSROOT/include" "$SYSROOT/lib" "$SYSROOT/lib/pkgconfig"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
SYSROOT=$(cd "$SYSROOT" && pwd)

export PATH="$ROOT/toolchain/bin:$PATH"

CC="$TARGET-gcc"
AR="$TARGET-ar"
RANLIB="$TARGET-ranlib"

CFLAGS="--sysroot=$SYSROOT -O2 -ffreestanding -fno-pic -fno-pie"
CFLAGS="$CFLAGS -I$LWIP_SRC/include -I$PORT_DIR/include"
CFLAGS="$CFLAGS -DWINVER=0x0600"

CORE_FILES="
core/init.c core/def.c core/dns.c core/inet_chksum.c
core/ip.c core/mem.c core/memp.c core/netif.c
core/pbuf.c core/raw.c core/stats.c core/sys.c
core/altcp.c core/altcp_alloc.c core/altcp_tcp.c
core/tcp.c core/tcp_in.c core/tcp_out.c
core/timeouts.c core/udp.c
"

CORE4_FILES="
core/ipv4/acd.c core/ipv4/autoip.c core/ipv4/dhcp.c
core/ipv4/etharp.c core/ipv4/icmp.c core/ipv4/igmp.c
core/ipv4/ip4_frag.c core/ipv4/ip4.c core/ipv4/ip4_addr.c
"

NETIF_FILES="
netif/ethernet.c netif/bridgeif.c netif/bridgeif_fdb.c
"

OBJS=""

compile() {
    local src="$LWIP_SRC/$1"
    local obj="$BUILD_DIR/$(echo "$1" | tr / _).o"
    mkdir -p "$(dirname "$obj")"
    echo "  CC    $1"
    "$CC" -c $CFLAGS "$src" -o "$obj"
    OBJS="$OBJS $obj"
}

for f in $CORE_FILES; do compile "$f"; done
for f in $CORE4_FILES; do compile "$f"; done
for f in $NETIF_FILES; do compile "$f"; done

echo "  AR    liblwip.a"
"$AR" rcs "$BUILD_DIR/liblwip.a" $OBJS
"$RANLIB" "$BUILD_DIR/liblwip.a"

echo "  INSTALL headers"
install -d "$SYSROOT/include/lwip"
install -d "$SYSROOT/include/lwip/apps"
install -d "$SYSROOT/include/lwip/priv"
install -d "$SYSROOT/include/lwip/prot"
install -d "$SYSROOT/include/netif"
install -d "$SYSROOT/include/compat/posix/arpa"
install -d "$SYSROOT/include/compat/posix/net"
install -d "$SYSROOT/include/compat/posix/sys"
install -d "$SYSROOT/include/compat/stdc"
install -d "$SYSROOT/include/arch"

install -m 644 "$LWIP_SRC/include/lwip"/*.h "$SYSROOT/include/lwip/"
install -m 644 "$LWIP_SRC/include/lwip/apps"/*.h "$SYSROOT/include/lwip/apps/"
install -m 644 "$LWIP_SRC/include/lwip/priv"/*.h "$SYSROOT/include/lwip/priv/"
install -m 644 "$LWIP_SRC/include/lwip/prot"/*.h "$SYSROOT/include/lwip/prot/"
install -m 644 "$LWIP_SRC/include/netif"/*.h "$SYSROOT/include/netif/"
install -m 644 "$LWIP_SRC/include/compat/posix/arpa"/*.h "$SYSROOT/include/compat/posix/arpa/"
install -m 644 "$LWIP_SRC/include/compat/posix/net"/*.h "$SYSROOT/include/compat/posix/net/"
install -m 644 "$LWIP_SRC/include/compat/posix/sys"/*.h "$SYSROOT/include/compat/posix/sys/"
install -m 644 "$LWIP_SRC/include/compat/stdc"/*.h "$SYSROOT/include/compat/stdc/"
install -m 644 "$PORT_DIR/include/arch/cc.h" "$SYSROOT/include/arch/cc.h"

echo "  INSTALL liblwip.a"
install -m 644 "$BUILD_DIR/liblwip.a" "$SYSROOT/lib/liblwip.a"

cat > "$SYSROOT/lib/pkgconfig/lwip.pc" <<'PCEOF'
prefix=/
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: lwip
Description: lwIP TCP/IP stack
Version: 2.2.1
Cflags: -I${includedir}
Libs: -L${libdir} -llwip
PCEOF

echo "lwIP built successfully"
