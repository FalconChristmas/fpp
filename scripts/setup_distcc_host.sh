#!/bin/bash
#####################################
# setup_distcc_host.sh
#
# Configure THIS machine as a distcc compile server ("helper") so that
# slower FPP devices can offload C/C++ compilation to it.  Intended to be
# run on a fast box (e.g. a Pi 5, a PocketBeagle2, or a desktop) that will
# crunch builds for the painfully slow single-core boards (BeagleBone
# Black, Pi Zero W, etc.).
#
# This is OPT-IN.  Running it does NOT change how any FPP device builds.
# A client only uses the helper when DISTCC_HOSTS is exported in its build
# environment (see the "CLIENT USAGE" notes printed at the end, and FPP's
# src/makefiles/common/setup.mk which switches to "ccache distcc" and
# disables the PCH automatically when DISTCC_HOSTS is set).
#
# ---------------------------------------------------------------------------
# Can ONE host serve both 32-bit and 64-bit clients?  YES -- with a caveat.
# ---------------------------------------------------------------------------
# distcc does not auto-detect the target.  On a cache miss the client ships
# PRE-PROCESSED source plus the *name* of the compiler it invoked; the
# server runs a compiler of that same name and ships the .o back.  So:
#
#   * The client must invoke a TARGET-TRIPLET compiler name, not plain
#     "g++".  A 32-bit board calls   arm-linux-gnueabihf-g++ ;
#     a 64-bit board calls           aarch64-linux-gnu-g++ .
#   * This host just needs a compiler of each of those names installed.
#     One armhf cross-compiler covers EVERY 32-bit ARM client (BBB armv7,
#     Pi Zero W armv6, 32-bit Pis, ...) because the -march/-mcpu tuning
#     rides in on the client's command line.  The native aarch64 compiler
#     covers every 64-bit client (PocketBeagle2, 64-bit Pis, ...).
#   * Only the cross-COMPILER is needed here -- NOT a target sysroot.
#     Headers are read during pre-processing on the client and libraries
#     are used during linking on the client; the server only turns .i into
#     .o, so no :armhf/:arm64 libraries are required on this host.
#
# THE CAVEAT: the compiler MAJOR VERSION must match between client and
# server (distcc/ccache assume the server reproduces the client compiler).
# This host installs the distro default (GCC 14 on Debian 13 / Trixie).
# That matches FPP OS image v2026-06 clients.  A client still on an older
# image (Debian 12 / GCC 12) will NOT match -- reimage it, or point it at a
# helper running the same release.  The script prints every installed
# compiler's version so you can confirm the match.
#
# SECURITY: distccd executes compile jobs received over TCP 3632.  Access
# is restricted to your LAN subnet (auto-detected, override with arg 1 or
# ALLOWEDNETS=...).  Do NOT expose 3632 to the internet.
#
# Usage:
#   sudo ./setup_distcc_host.sh                 # auto-detect LAN, install, start
#   sudo ./setup_distcc_host.sh 192.168.7.0/24  # force the allowed network(s)
#   sudo ALLOWEDNETS="10.0.0.0/24 10.0.1.0/24" ./setup_distcc_host.sh
#   sudo ./setup_distcc_host.sh disable         # stop & disable the helper
#####################################

set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: must be run as root (use sudo)." >&2
    exit 1
fi

# Pick the systemd unit name distcc ships on this distro.
distcc_unit() {
    if systemctl list-unit-files 2>/dev/null | grep -q '^distccd\.service'; then
        echo distccd
    else
        echo distcc
    fi
}

# ---- disable path -----------------------------------------------------------
if [ "${1:-}" = "disable" ]; then
    unit="$(distcc_unit)"
    echo "Stopping and disabling ${unit}..."
    systemctl stop "${unit}" 2>/dev/null || true
    systemctl disable "${unit}" 2>/dev/null || true
    if [ -f /etc/default/distcc ]; then
        sed -i 's/^STARTDISTCC=.*/STARTDISTCC="false"/' /etc/default/distcc || true
    fi
    echo "distcc helper disabled.  (Packages left installed.)"
    exit 0
fi

# ---- derive the network/prefix from a CIDR (any prefix length) --------------
cidr_network() {
    local cidr="$1" ip prefix a b c d ipnum mask net
    ip="${cidr%/*}"; prefix="${cidr#*/}"
    IFS=. read -r a b c d <<<"$ip"
    ipnum=$(( (a<<24)|(b<<16)|(c<<8)|d ))
    mask=$(( (0xFFFFFFFF << (32-prefix)) & 0xFFFFFFFF ))
    net=$(( ipnum & mask ))
    printf "%d.%d.%d.%d/%d" $(((net>>24)&255)) $(((net>>16)&255)) $(((net>>8)&255)) $((net&255)) "$prefix"
}

# ---- determine allowed networks --------------------------------------------
if [ -n "${ALLOWEDNETS:-}" ]; then
    ALLOWED="127.0.0.1 ${ALLOWEDNETS}"
elif [ -n "${1:-}" ]; then
    ALLOWED="127.0.0.1 $*"
else
    nets=""
    while read -r cidr; do
        [ -n "$cidr" ] && nets="$nets $(cidr_network "$cidr")"
    done < <(ip -o -4 addr show scope global 2>/dev/null | awk '{print $4}')
    if [ -z "$nets" ]; then
        echo "ERROR: could not auto-detect a LAN subnet; pass one as arg 1." >&2
        exit 1
    fi
    # de-dup
    ALLOWED="127.0.0.1$(echo "$nets" | tr ' ' '\n' | sort -u | tr '\n' ' ' | sed 's/^/ /;s/ *$//')"
fi
echo "==> Allowed networks: ${ALLOWED}"

# ---- packages: distcc + the cross compilers we want to serve ----------------
# We want to be able to serve both 32-bit ARM (armhf) and 64-bit ARM (arm64)
# clients.  Install a compiler for any target triplet not already present.
ensure_triplet() {
    local triplet="$1" pkgs=""
    command -v "${triplet}-g++" >/dev/null 2>&1 || pkgs="g++-${triplet}"
    command -v "${triplet}-gcc" >/dev/null 2>&1 || pkgs="${pkgs} gcc-${triplet}"
    if [ -n "${pkgs// /}" ]; then
        echo "==> Installing for ${triplet}: ${pkgs}"
        DEBIAN_FRONTEND=noninteractive apt-get install -y ${pkgs}
    else
        echo "==> ${triplet} compiler already present (native or cross)."
    fi
}

echo "==> apt-get update"
apt-get update -qq

echo "==> Installing distcc"
DEBIAN_FRONTEND=noninteractive apt-get install -y distcc

# Serve the two ABIs FPP boards use.  Each is the native compiler on the
# matching host, or a cross package elsewhere -- ensure_triplet figures it out.
ensure_triplet arm-linux-gnueabihf   # 32-bit ARM: BBB, Pi Zero W, 32-bit Pis
ensure_triplet aarch64-linux-gnu     # 64-bit ARM: PocketBeagle2, 64-bit Pis

# ---- write /etc/default/distcc ---------------------------------------------
JOBS="$(nproc)"
echo "==> Writing /etc/default/distcc (jobs=${JOBS}, nice=10)"
cat > /etc/default/distcc <<EOF
# Managed by FPP scripts/setup_distcc_host.sh
STARTDISTCC="true"
# Restrict who may submit compile jobs (LAN only -- never expose 3632 to WAN).
ALLOWEDNETS="${ALLOWED}"
# Bind on all interfaces; ALLOWEDNETS is the real access control.
LISTENER="0.0.0.0"
# Parallel compile slots handed out to clients.
JOBS="${JOBS}"
# Keep the box responsive (fppd etc.) -- compiles run at low priority.
NICE="10"
# We hand out hosts explicitly via DISTCC_HOSTS on clients, not mDNS.
ZEROCONF="false"
EOF

# ---- enable + (re)start -----------------------------------------------------
UNIT="$(distcc_unit)"
echo "==> Enabling and starting ${UNIT}"
systemctl enable "${UNIT}" >/dev/null 2>&1 || true
systemctl restart "${UNIT}"

sleep 1
echo
echo "============================================================"
if systemctl is-active --quiet "${UNIT}"; then
    echo "distcc helper is RUNNING (${UNIT})."
else
    echo "WARNING: ${UNIT} is not active -- check: journalctl -u ${UNIT}" >&2
fi
ss -ltn 2>/dev/null | grep -q ':3632' && echo "Listening on TCP 3632." || \
    echo "NOTE: did not see a listener on 3632 yet."

echo
echo "Installed compilers on this host (clients MUST match the major version):"
for t in aarch64-linux-gnu arm-linux-gnueabihf; do
    if command -v "${t}-g++" >/dev/null 2>&1; then
        printf "  %-22s %s\n" "${t}-g++" "$(${t}-g++ -dumpfullversion 2>/dev/null)"
    fi
done

HOSTIP="$(ip -o -4 addr show scope global 2>/dev/null | awk '{print $4}' | head -1 | cut -d/ -f1)"
HOSTNAME_S="$(hostname)"
echo
echo "------------------------------------------------------------"
echo "CLIENT USAGE (run on the SLOW board you want to speed up):"
echo
echo "  # 32-bit boards (BeagleBone Black, Pi Zero W, 32-bit Pi):"
echo "  export DISTCC_HOSTS=\"${HOSTNAME_S}:3632/${JOBS},lzo\"   # or ${HOSTIP}:3632/${JOBS},lzo"
echo "  cd /opt/fpp/src"
echo "  make -j\$(( ${JOBS} + 2 )) CXXCOMPILER=arm-linux-gnueabihf-g++ CCOMPILER=arm-linux-gnueabihf-gcc"
echo
echo "  # 64-bit boards (PocketBeagle2, 64-bit Pi):"
echo "  export DISTCC_HOSTS=\"${HOSTNAME_S}:3632/${JOBS},lzo\""
echo "  cd /opt/fpp/src"
echo "  make -j\$(( ${JOBS} + 2 )) CXXCOMPILER=aarch64-linux-gnu-g++ CCOMPILER=aarch64-linux-gnu-gcc"
echo
echo "  Setting DISTCC_HOSTS makes FPP use 'ccache distcc' and disables the"
echo "  PCH automatically.  -jN should be a little above this host's job count"
echo "  so the client keeps the helper fed while it pre-processes locally."
echo "  Watch live distribution with:  DISTCC_VERBOSE=0 distccmon-text 1"
echo
echo "To turn the helper back off:  sudo $0 disable"
echo "============================================================"
