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
fi
