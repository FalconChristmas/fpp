#!/bin/bash
#####################################
# Upgrade 141: Pin rpi-swap to plain zram so boot cannot hang on the
#              /var/swap loop device
#
# Raspberry Pi OS Trixie replaced dphys-swapfile with rpi-swap, whose default
# mechanism ("auto" -> "zram+file") attaches /var/swap to a loop device and
# hands it to zram0 as a WRITEBACK target.  It is never swapped on, so it adds
# no usable swap -- DiskBackedSwapKB in scripts/functions already ignores it
# for exactly that reason.  What it does add is a unit on the boot critical
# path: swap.target -> dev-zram0.swap -> systemd-zram-setup@zram0 ->
# rpi-setup-loop@var-swap, and sysinit.target is ordered after swap.target, so
# that one unit gates the whole boot.
#
# rpi-setup-loop@.service ends in ExecStartPost=rpi-wait-backingfile-symlink,
# which waits for udev's /dev/disk/by-backingfile/var-swap symlink with an
# inotify loop that arms its watch AFTER testing for the symlink.  Lose that
# race -- the symlink lands in the gap, or udev unlinks and recreates it while
# re-processing loop0 -- and the script blocks on an event that will never come
# again.  The unit is Type=oneshot, whose TimeoutStartSec defaults to infinity,
# so the boot stops dead at "A start job is running for
# rpi-setup-loop@var-swap.service", with no console and no ssh.  Only a power
# cycle clears it, and because it is a race the next boot usually succeeds,
# which makes it look sporadic and unrelated to whatever changed last.
#
# Switching the mechanism to plain "zram" removes the loop unit and the
# writeback timer.  zram itself is untouched: same generator, same sizing, same
# dev-zram0.swap.  rpi-swap also cleans up a leftover /var/swap under this
# mechanism, via rpi-remove-swap-file@, pulled in by multi-user.target well off
# the boot path.
#
# Fresh installs get this from configure_swap() in SD/FPP_Install.sh; this
# brings already-installed systems over.  Idempotent -- safe to re-run.
#
# The generator only runs at boot (or on daemon-reload), so this takes effect
# on the device's next reboot.  Nothing is torn down here: unhooking a live
# writeback device from a running zram0 is not worth the risk to fix a boot
# path that is already past.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 141: Pin rpi-swap to plain zram (no /var/swap loop device)"

# rpi-swap is Raspberry Pi OS Trixie and later.  Bookworm Pi images, BeagleBone
# and desktop installs have no /etc/rpi/swap.conf and nothing to configure.
if [ ! -f /etc/rpi/swap.conf ]; then
    echo "  rpi-swap not installed - skipping"
    exit 0
fi

DROPIN="/etc/rpi/swap.conf.d/10-fpp-zram-only.conf"

if [ -f "${DROPIN}" ]; then
    echo "  ${DROPIN} already present"
    exit 0
fi

mkdir -p /etc/rpi/swap.conf.d
cat > "${DROPIN}" <<'SWAP_EOF'
# Installed by FPP. See configure_swap() in SD/FPP_Install.sh.
[Main]
Mechanism=zram
SWAP_EOF

echo "  Wrote ${DROPIN} - applies on the next reboot"

exit 0
