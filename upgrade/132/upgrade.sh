#!/bin/bash
#####################################
# Upgrade 132: Pin MultiSyncUnicast off when the setting is absent
#
# Upgrade 122 pinned existing multisync users to multicast when FPP 10 changed
# the default send method, but its guard was all-or-nothing:
#
#   if Multicast != 1 and Broadcast != 1 and Unicast != 1:
#       setSetting MultiSyncMulticast 1
#       setSetting MultiSyncUnicast 0
#
# MultiSyncUnicast was only ever written on that one branch. A user who had
# already picked multicast or broadcast explicitly -- which is most of them,
# and everyone configured before the unicast mode existed -- skips the branch
# entirely, so the key stays ABSENT.
#
# Absent is not neutral. It resolves differently depending on who is asking:
#
#   www/common.php IfSettingEqualPrint() -> settings.json "default": 1
#   src/settings.cpp LoadSettingsInfo()  -> settings.json "default": 1
#   getSetting / getRawSettingInt        -> empty / 0, the file is read directly
#
# So those boxes show "Send MultiSync to ALL KNOWN remotes via Unicast" ticked
# in the UI *and* fppd genuinely unicasts to every discovered remote, on top of
# the multicast or broadcast the user actually chose -- remotes receive two
# copies of every sync packet, per frame. Meanwhile upgrade 122, reading raw,
# saw nothing to correct.
#
# Writing the key makes the settings file, the web UI and fppd agree.
#
# Only ABSENCE is corrected here. An explicit 0 or 1 is the user's own choice
# and is left alone, so this is safe to re-run and safe for anyone who has
# deliberately turned unicast-to-all on.
#
# The FPPOS-reflash path does not run these scripts; it is covered by
# migrateMultiSyncDefaultToMulticast() in src/boot/FPPINIT_Config.cpp, which
# carries the same logic. Keep the two in step.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 132: Pin MultiSyncUnicast off where it was never recorded"

if [ "$(getSetting MultiSyncEnabled)" != "1" ]; then
    exit 0
fi

if [ -z "$(getSetting MultiSyncUnicast)" ]; then
    setSetting MultiSyncUnicast 0
    echo "  MultiSyncUnicast was not set - pinned to 0 to match this system's existing sync method"
else
    echo "  MultiSyncUnicast is already set explicitly - leaving it alone"
fi

exit 0
