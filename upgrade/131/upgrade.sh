#!/bin/bash
#####################################
# Upgrade 131: Stop a stock 16K-page Pi kernel from hijacking the boot
#
# FPP's Pi images ship exactly one kernel -- the 4K-page kernel8.img from
# rpi-update -- and strip every other /lib/modules tree, including the stock
# Debian *-rpt-rpi-2712 one. A Pi5/CM5 picks its kernel by FILENAME and
# prefers /boot/firmware/kernel_2712.img over kernel8.img, so if a
# kernel_2712.img ever lands on the boot partition the box boots a 16K-page
# stock kernel that has no modules on this image at all. The symptoms are
# nowhere near the cause:
#
#   * no ipv6 module  -> apache dies on "Listen [::]:80" and the web UI is
#                        gone ("AH00078: failed to get a socket for ::")
#   * no zram module  -> dev-zram0.device times out and adds 90s to boot
#
# Two things put that file there, and this closes both:
#
#   1. raspi-firmware's kernel postinst hook copies vmlinuz ->
#      kernel_2712.img whenever KERNEL is "auto". Images built before
#      2026-07-17 ship the stock /etc/default/raspi-firmware with KERNEL
#      unset, so any apt run that touches a linux-image recreates it.
#   2. Even with KERNEL=none, apt is still free to *install* a newer
#      linux-image-*-rpi-2712. Pin it out; nothing depends on those packages.
#
# Gated on FPP actually managing the kernels, detected by the absence of any
# *-2712 module tree. A manual FPP_Install.sh on top of stock Raspberry Pi OS
# keeps its own 2712 kernel AND its modules, and must not be touched here.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 131: Prevent a stock 16K-page kernel from hijacking the boot"

if [ "${FPPPLATFORM}" != "Raspberry Pi" ] || [ ! -d /boot/firmware ]; then
    exit 0
fi

# FPP-managed kernels? Then no stock 2712 module tree exists. If one does,
# this is a stock-Raspberry-Pi-OS install where the 2712 kernel is legitimate.
if ls -d /lib/modules/*-2712* >/dev/null 2>&1 || ls -d /lib/modules/*rpt-rpi-2712 >/dev/null 2>&1; then
    echo "  Stock 2712 modules are present - this install manages its own kernel, skipping"
    exit 0
fi

# 1. Refuse the 16K-page kernel packages outright.
cat > /etc/apt/preferences.d/fpp-no-2712-kernel <<'APT_PIN_EOF'
# FPP ships only the 4K-page kernel8.img and its matching /lib/modules tree.
# The stock linux-image-*-rpi-2712 packages are 16K-page kernels
# (CONFIG_ARM64_16K_PAGES) that some FPP libs are incompatible with, and
# installing one drops a kernel_2712.img into /boot/firmware which the Pi
# firmware then PREFERS over kernel8.img. Refuse them outright.
Package: linux-image-rpi-2712 linux-image-*-rpi-2712
Pin: release *
Pin-Priority: -1
APT_PIN_EOF
echo "  Pinned linux-image-*-rpi-2712 out of apt"

# 2. Stop raspi-firmware's kernel hook from copying a stock vmlinuz in.
if ! grep -q '^KERNEL=none' /etc/default/raspi-firmware 2>/dev/null; then
    cat > /etc/default/raspi-firmware <<'RASPI_FW_EOF'
# FPP manages its own kernels in /boot/firmware; "auto" would let
# raspi-firmware overwrite them with a stock Debian kernel whose modules
# are not present on this image.
KERNEL=none
RASPI_FW_EOF
    echo "  Set KERNEL=none in /etc/default/raspi-firmware"
fi

# 3. Defuse one that is already sitting on the boot partition. Only ever with
#    kernel8.img present to fall back to.
if [ -f /boot/firmware/kernel_2712.img ]; then
    if [ -f /boot/firmware/kernel8.img ]; then
        rm -f /boot/firmware/kernel_2712.img
        sync
        echo "  Removed a stale /boot/firmware/kernel_2712.img (the Pi will boot kernel8.img)"
        # If we are already running it, the modules for the running kernel are
        # missing and only a reboot recovers.
        if [ ! -d "/lib/modules/$(uname -r)" ]; then
            echo "  NOTE: this box is currently running $(uname -r), which has no"
            echo "        modules on this image. A reboot is required to recover."
            setSetting rebootFlag 1
        fi
    else
        echo "  WARNING: kernel_2712.img is present but kernel8.img is missing;"
        echo "           leaving it alone so the box stays bootable."
    fi
fi

exit 0
