#!/bin/bash
#####################################
# Upgrade 144: Prevent HDMI CEC At Boot by default
#
# Issue #2938: FPP 10 (Trixie, 6.18 vc4-kms-v3d) wakes CEC projectors/TVs
# at boot via vc4-hdmi-cec Active Source. The new DisableHDMICECInit
# toggle adds hdmi_ignore_cec_init=1 to config.txt when enabled.
#
# For fresh installs the new default in www/settings.json (1) already
# applies. For existing installs that have never saved the setting
# (no line in /home/fpp/media/settings), pin it to 1 now so the
# projector stays in standby after an upgrade. Explicit 0 stays 0.
#
# Handles normal in-place upgrades via scripts/upgrade_config.
# FPPOS-reflash / restore path is handled separately in
# src/boot/FPPINIT_Config.cpp:checkConfigMigrations().
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ -z "$(getSetting DisableHDMICECInit)" ]; then
    setSetting DisableHDMICECInit 1
    # Pre-seed the config.txt block so the next boot does not trigger
    # the extra reboot that setupHDMICECConfig(rebootIfChanged=true) would
    # otherwise do for every Pi. Idempotent and Pi-only.
    BOOTCONF=""
    if [ -f /boot/firmware/config.txt ]; then
        BOOTCONF="/boot/firmware/config.txt"
    elif [ -f /boot/config.txt ]; then
        BOOTCONF="/boot/config.txt"
    fi
    if [ -n "$BOOTCONF" ] && ! grep -q "FPP HDMI CEC - BEGIN" "$BOOTCONF" 2>/dev/null; then
        # Insert ahead of cape variant block if present, else append
        if grep -q "FPP Cape Overlay Variants - BEGIN" "$BOOTCONF" 2>/dev/null; then
            awk 'BEGIN{p=0} /FPP Cape Overlay Variants - BEGIN/ && p==0 {print "# FPP HDMI CEC - BEGIN (managed by fppinit, do not edit)"; print "[all]"; print "hdmi_ignore_cec_init=1"; print "[all]"; print "# FPP HDMI CEC - END"; print ""; p=1} {print}' "$BOOTCONF" > "${BOOTCONF}.tmp" && mv "${BOOTCONF}.tmp" "$BOOTCONF"
        else
            printf "\n# FPP HDMI CEC - BEGIN (managed by fppinit, do not edit)\n[all]\nhdmi_ignore_cec_init=1\n[all]\n# FPP HDMI CEC - END\n" >> "$BOOTCONF"
        fi
    fi
fi
