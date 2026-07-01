#!/bin/bash
#####################################
# setup_nocc_host.sh
#
# Configure THIS machine as an FPP *nocc* compile server ("helper") so slower
# FPP devices can offload C/C++ compilation to it. Run on a fast box (a Pi 5,
# a desktop) that will crunch builds for painfully slow boards (BeagleBone
# Black, Pi Zero W, ...). This is the nocc counterpart to setup_distcc_host.sh.
#
# WHY NOCC (vs distcc): distcc makes the slow CLIENT pre-process every file
# locally, which alone saturates a single core. nocc does a light include-scan
# on the client and pre-processes + compiles on the helper, so the helper
# actually gets fed. Measured ~3x faster full builds on a BBB.
#
# This is OPT-IN. A client only uses the helper when its build env points at it
# (NOCC_SERVERS=thishost:43210, with CCACHE_PREFIX=nocc). The nocc binaries and
# the (disabled) nocc-server unit come from the FPP apt repo:  apt-get install nocc
#
# ---------------------------------------------------------------------------
# Serving both 32-bit and 64-bit clients: YES, one host.
# ---------------------------------------------------------------------------
# Like distcc, nocc runs whatever compiler NAME the client invokes. A 32-bit
# board calls arm-linux-gnueabihf-g++; a 64-bit board calls aarch64-linux-gnu-g++.
# This host just needs a compiler of each of those names -- NO target sysroot
# (nocc ships the headers it needs and caches them). One armhf cross-compiler
# covers every 32-bit ARM client; the aarch64 compiler covers every 64-bit one.
#
# THE CAVEAT (same as distcc): the compiler MAJOR VERSION must match between
# client and server. This host installs the distro default (GCC 14 on Debian 13
# / Trixie), matching FPP OS image v2026-06 clients. The script prints versions.
#
# SECURITY: nocc-server has NO built-in access control and listens on TCP 43210.
# Keep it LAN-only (firewall it). Pass NOCC_FIREWALL=1 to install an nftables
# rule that allows 43210 only from your LAN. NEVER expose 43210 to the internet.
#
# Usage:
#   sudo ./setup_nocc_host.sh                    # ensure compilers, enable+start
#   sudo PORT=43210 ./setup_nocc_host.sh         # override port
#   sudo NOCC_FIREWALL=1 ./setup_nocc_host.sh    # + LAN-only nftables rule
#   sudo NOCC_FIREWALL=1 ./setup_nocc_host.sh 192.168.7.0/24   # force LAN CIDR(s)
#   sudo ./setup_nocc_host.sh disable            # stop & disable the helper
#####################################

set -euo pipefail

PORT="${PORT:-43210}"

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: must be run as root (use sudo)." >&2
    exit 1
fi

# ---- disable path -----------------------------------------------------------
if [ "${1:-}" = "disable" ]; then
    echo "Stopping and disabling nocc-server..."
    systemctl disable --now nocc-server 2>/dev/null || true
    echo "nocc helper disabled.  (Package left installed.)"
    exit 0
fi

# ---- ensure the nocc package (server binary + unit) is present --------------
if ! command -v nocc-server >/dev/null 2>&1; then
    echo "==> nocc-server not found; attempting 'apt-get install nocc'"
    apt-get update -qq || true
    DEBIAN_FRONTEND=noninteractive apt-get install -y nocc || true
fi
if ! command -v nocc-server >/dev/null 2>&1; then
    echo "ERROR: nocc-server not installed. Add FPP's apt repo and 'apt-get install nocc'." >&2
    echo "       (nocc is not in Debian; it ships from FPP's own repository.)" >&2
    exit 1
fi

# ---- cross compilers we serve (native or cross; version must match clients) --
ensure_triplet() {
    local triplet="$1" pkgs=""
    command -v "${triplet}-g++" >/dev/null 2>&1 || pkgs="g++-${triplet}"
    command -v "${triplet}-gcc" >/dev/null 2>&1 || pkgs="${pkgs} gcc-${triplet}"
    if [ -n "${pkgs// /}" ]; then
        echo "==> Installing for ${triplet}: ${pkgs}"
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get install -y ${pkgs}
    else
        echo "==> ${triplet} compiler already present (native or cross)."
    fi
}
ensure_triplet arm-linux-gnueabihf   # 32-bit ARM: BBB, Pi Zero W, 32-bit Pis
ensure_triplet aarch64-linux-gnu     # 64-bit ARM: PocketBeagle2, 64-bit Pis

# ---- apply port override to the packaged env file ---------------------------
ENVF=/etc/default/nocc-server
if [ "$PORT" != "43210" ] && [ -f "$ENVF" ]; then
    echo "==> Setting nocc-server port to ${PORT} in ${ENVF}"
    sed -i -E "s/-port [0-9]+/-port ${PORT}/" "$ENVF"
fi

# ---- optional LAN-only firewall (nocc-server has no ACL of its own) ----------
if [ "${NOCC_FIREWALL:-0}" = "1" ]; then
    if command -v nft >/dev/null 2>&1; then
        # Determine LAN CIDR(s): arg1/env, else auto-detect.
        NETS="${1:-${ALLOWEDNETS:-}}"
        if [ -z "$NETS" ]; then
            NETS="$(ip -o -4 addr show scope global 2>/dev/null | awk '{print $4}' | tr '\n' ' ')"
        fi
        echo "==> Installing nftables rule: allow ${PORT}/tcp only from: ${NETS} (+loopback)"
        nft add table inet fpp_nocc 2>/dev/null || true
        nft flush table inet fpp_nocc 2>/dev/null || true
        nft add chain inet fpp_nocc input '{ type filter hook input priority -10 ; }' 2>/dev/null || true
        nft add rule inet fpp_nocc input iif lo accept 2>/dev/null || true
        for net in $NETS; do
            nft add rule inet fpp_nocc input ip saddr "$net" tcp dport "$PORT" accept 2>/dev/null || true
        done
        nft add rule inet fpp_nocc input tcp dport "$PORT" drop 2>/dev/null || true
    else
        echo "WARNING: NOCC_FIREWALL=1 but 'nft' not found; install nftables or firewall port ${PORT} yourself." >&2
    fi
else
    echo "NOTE: nocc-server has NO access control. Restrict TCP ${PORT} to your LAN"
    echo "      (re-run with NOCC_FIREWALL=1, or add your own firewall rule)."
fi

# ---- enable + (re)start -----------------------------------------------------
echo "==> Enabling and starting nocc-server"
systemctl daemon-reload
systemctl enable --now nocc-server
sleep 1

echo
echo "============================================================"
if systemctl is-active --quiet nocc-server; then
    echo "nocc helper is RUNNING (nocc-server)."
else
    echo "WARNING: nocc-server is not active -- check: journalctl -u nocc-server" >&2
fi
ss -ltn 2>/dev/null | grep -q ":${PORT}" && echo "Listening on TCP ${PORT}." || \
    echo "NOTE: did not see a listener on ${PORT} yet."

echo
echo "Installed compilers on this host (clients MUST match the major version):"
for t in aarch64-linux-gnu arm-linux-gnueabihf; do
    if command -v "${t}-g++" >/dev/null 2>&1; then
        printf "  %-22s %s\n" "${t}-g++" "$(${t}-g++ -dumpfullversion 2>/dev/null)"
    fi
done

HOSTNAME_S="$(hostname)"
cat <<EOF

------------------------------------------------------------
CLIENT USAGE (on the SLOW board you want to speed up):

  Install the nocc package (from FPP's apt repo):
      sudo apt-get install nocc

  Point the FPP build at this helper AND keep ccache (so the shared/global
  ccache still works -- nocc is just the compile backend):
      export CCACHE_PREFIX=nocc
      export NOCC_SERVERS=${HOSTNAME_S}:${PORT}
      export NOCC_GO_EXECUTABLE=/usr/bin/nocc-daemon
      cd /opt/fpp/src && make -j6 CXXCOMPILER=<triplet>-g++ ...

  <triplet> is arm-linux-gnueabihf (32-bit) or aarch64-linux-gnu (64-bit).
  nocc has no host auto-discovery (unlike distcc's mDNS); list this helper
  explicitly in NOCC_SERVERS.

To turn the helper back off:  sudo $0 disable
============================================================
EOF
