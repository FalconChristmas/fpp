#!/bin/bash
#####################################
# Upgrade 111: Fix HDMI audio on Raspberry Pi Zero 2 W
#
# On Pi Zero 2 W with vc4-kms-v3d, HDMI audio is provided by the vc4-hdmi
# kernel driver.  The legacy snd_bcm2835 driver is disabled via
# dtparam=audio=off in the [pi02] config.txt section, but two problems
# can affect existing installs:
#
#   1. The [pi02] config.txt section may be missing dtparam=audio=off,
#      hdmi_force_hotplug=1, and hdmi_drive=2 (added in a later FPP image).
#
#   2. cmdline.txt contains snd_bcm2835.enable_headphones=1 and
#      snd_bcm2835.enable_hdmi=0 which were added for all Pi models.
#      On Pi Zero 2 W these params prevent HDMI audio from working because
#      they conflict with the vc4-hdmi audio path.
#
# This upgrade is a no-op on all non-Pi-Zero-2-W platforms.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 111: Fix HDMI audio on Raspberry Pi Zero 2 W"
echo "============================================================"

# Only applies to Raspberry Pi
if [ "${FPPPLATFORM}" != "Raspberry Pi" ]; then
    echo "  Not a Raspberry Pi — nothing to do."
    exit 0
fi

# Detect Pi model
MODEL=""
if [ -f /proc/device-tree/model ]; then
    MODEL=$(tr -d '\0' < /proc/device-tree/model)
fi

if ! echo "${MODEL}" | grep -q "Zero 2"; then
    echo "  Model: '${MODEL}' — not a Pi Zero 2 W, nothing to do."
    exit 0
fi

echo "  Model: '${MODEL}'"

BOOTDIR="/boot/firmware"
CONFIG="${BOOTDIR}/config.txt"
CMDLINE="${BOOTDIR}/cmdline.txt"
CHANGED=0

#######################################
# 1. Ensure [pi02] section in config.txt has the required audio entries.
#    Use Python to parse the ini-style sections properly so we can detect
#    existing keys and insert missing ones in the correct order without
#    creating duplicates.
#######################################
echo "  Checking ${CONFIG} [pi02] section..."

python3 - "${CONFIG}" << 'PYEOF'
import sys

config_file = sys.argv[1]
with open(config_file) as f:
    lines = f.readlines()

REQUIRED = [
    ('dtparam=audio',     'dtparam=audio=off'),
    ('hdmi_force_hotplug', 'hdmi_force_hotplug=1'),
    ('hdmi_drive',         'hdmi_drive=2'),
]

# Find what's already in the [pi02] block
in_pi02 = False
present = set()
for line in lines:
    s = line.strip()
    if s == '[pi02]':
        in_pi02 = True
        continue
    if in_pi02 and s.startswith('['):
        break
    if in_pi02:
        for key, _ in REQUIRED:
            if s.startswith(key):
                present.add(key)

missing = [(k, v) for k, v in REQUIRED if k not in present]
if not missing:
    print('    All required entries already present')
    sys.exit(0)

for k, v in missing:
    print(f'    Adding {v}')

# Insert missing lines right after the [pi02] line
out = []
inserted = False
for line in lines:
    out.append(line)
    if not inserted and line.strip() == '[pi02]':
        for _, v in missing:
            out.append(v + '\n')
        inserted = True

with open(config_file, 'w') as f:
    f.writelines(out)

sys.exit(1)  # exit 1 = changes were made
PYEOF

PY_RC=$?
if [ "${PY_RC}" = "1" ]; then
    CHANGED=1
fi

#######################################
# 2. Remove snd_bcm2835 params from cmdline.txt
#######################################
echo "  Checking ${CMDLINE} for snd_bcm2835 params..."

if grep -q 'snd_bcm2835\.' "${CMDLINE}"; then
    echo "    Removing snd_bcm2835.enable_headphones and snd_bcm2835.enable_hdmi"
    sed -i 's/ snd_bcm2835\.enable_headphones=[0-9]*//g' "${CMDLINE}"
    sed -i 's/ snd_bcm2835\.enable_hdmi=[0-9]*//g' "${CMDLINE}"
    sed -i 's/  */ /g' "${CMDLINE}"
    CHANGED=1
else
    echo "    No snd_bcm2835 params found — already clean"
fi

#######################################
# 3. Signal reboot if anything changed
#######################################
if [ "${CHANGED}" = "1" ]; then
    echo ""
    echo "  Changes made — a reboot is required for them to take effect."
    setSetting rebootFlag 1
else
    echo ""
    echo "  No changes needed."
fi

echo ""
echo "Upgrade 111 complete."
exit 0
