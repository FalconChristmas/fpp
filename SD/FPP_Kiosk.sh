#!/bin/bash

. /opt/fpp/scripts/common
. /opt/fpp/scripts/functions

# This script runs in two situations:
#   1. Interactively from the UI (Enable Kiosk button), when the box is fully up.
#   2. Early at boot via the fpp-install-kiosk service after a fppos upgrade.
# In case 2 the apt mirror may not be reachable yet and another process (e.g.
# unattended-upgrades) may hold the apt/dpkg lock. We can't simply order the
# service after network-online.target because FPP deliberately disables
# systemd-networkd-wait-online (controllers often run standalone with no
# network), so that target is never meaningfully reached. Instead, make apt
# itself resilient and rely on the install being retried on the next boot if
# it still can't complete (see the package-verification guard below).

# Wait up to 60s for the apt/dpkg lock rather than failing immediately if
# another process is mid-install at boot.
APT_OPTS="-o DPkg::Lock::Timeout=60"

# Retry "apt-get update" a few times - a single transient failure at boot
# should not abort the whole kiosk install. If it never succeeds we still try
# the installs below (the mirror may already be cached) and ultimately fall
# back to the retry-on-next-boot guard.
for i in 1 2 3; do
    apt-get $APT_OPTS update && break
    echo "apt-get update failed (attempt $i), retrying..."
    sleep 5
done

if  [ "${FPPPLATFORM}" = "Raspberry Pi" ]; then
    # need this to initialize the GPU on the Pi5
    apt-get $APT_OPTS install -y gldriver-test
fi
# Install the full functional X stack. Do NOT add --no-install-recommends
# here: Debian expresses the pieces that make X actually usable -- the setuid
# console wrapper (xserver-xorg-legacy), the input drivers (libinput, via
# xserver-xorg-input-all) and GL/DRI (libgl1-mesa-dri) -- as Recommends or
# alternative Depends on xserver-xorg. The Trixie FPP base image ships no X at
# all, so stripping recommends here leaves a half-working X: no console
# wrapper (startx is refused), no touch/keyboard/mouse, no GL. We also name
# the must-haves explicitly so an upstream packaging change cannot silently
# drop them out from under us.
apt-get $APT_OPTS install -y \
    xserver-xorg xserver-xorg-legacy xserver-xorg-input-libinput \
    x11-xserver-utils xinit openbox libgl1-mesa-dri
# Try chromium first (Debian Trixie+), fall back to chromium-browser (older versions)
apt-get $APT_OPTS install -y --no-install-recommends chromium || apt-get $APT_OPTS install -y --no-install-recommends chromium-browser
apt-get clean

# Verify the packages we depend on actually installed before marking kiosk as
# installed. This script can run early at boot (via the fpp-install-kiosk
# service after a fppos upgrade) when the network/apt mirror may not yet be
# reachable, so the apt-get calls above can silently fail. If we wrote the
# /etc/fpp/kiosk marker anyway, checkInstallKiosk()/installKiosk() would treat
# kiosk as installed and never retry, leaving a non-working kiosk that only an
# untick/re-tick (which re-runs this script) could fix. Bail out instead so the
# install is retried on the next boot.
if ! command -v chromium > /dev/null 2>&1 && ! command -v chromium-browser > /dev/null 2>&1; then
    echo "ERROR: chromium failed to install - leaving kiosk uninstalled so it retries on next boot"
    exit 1
fi
if ! command -v xinit > /dev/null 2>&1 || ! command -v openbox > /dev/null 2>&1; then
    echo "ERROR: xinit/openbox failed to install - leaving kiosk uninstalled so it retries on next boot"
    exit 1
fi
# These come via Recommends/alternative-Depends and have silently gone missing
# before. Without the setuid wrapper, non-root startx from the console is
# refused; without an input driver, X comes up with no touch/keyboard/mouse.
# Treat either as a failed install so it retries rather than leaving a
# half-working kiosk marked as installed.
if [ ! -e /usr/lib/xorg/Xorg.wrap ]; then
    echo "ERROR: xserver-xorg-legacy (Xorg.wrap) missing - leaving kiosk uninstalled so it retries on next boot"
    exit 1
fi
if ! ls /usr/lib/xorg/modules/input/*.so > /dev/null 2>&1; then
    echo "ERROR: no X input driver installed - leaving kiosk uninstalled so it retries on next boot"
    exit 1
fi

cat > /etc/xdg/openbox/autostart <<EOF
/opt/fpp/scripts/start_kiosk.sh
EOF

echo "Enabling AutoLogin"
# Ensure the directory exists
mkdir -p /etc/systemd/system/getty@tty1.service.d
cat > /etc/systemd/system/getty@tty1.service.d/autologin.conf << EOF
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin fpp --noclear %I \$TERM

EOF

echo "Enabling startup of chromium"
# Only append the startx line if it isn't already present. A fppos upgrade
# preserves /home/fpp (it is excluded from the root rsync), so a re-run of this
# script would otherwise accumulate duplicate startx lines in .bashrc.
if ! grep -q 'XDG_VTNR -eq 1 ]] && startx' /home/fpp/.bashrc 2>/dev/null; then
    cat >> /home/fpp/.bashrc << EOF

[[ -z \$DISPLAY && \$XDG_VTNR -eq 1 ]] && startx --

EOF
    chown fpp:fpp /home/fpp/.bashrc
fi

echo "kiosk" > /etc/fpp/kiosk

mkdir -p /etc/chromium/policies/managed/
cat > /etc/chromium/policies/managed/policy.json <<EOF
{"RestoreOnStartup": 4,"RestoreOnStartupURLs": ["http://localhost/"]}
EOF

setSetting Kiosk 1
setSetting rebootFlag 1
