#!/bin/bash
#####################################
# Upgrade 148: retire the legacy "master" / "bridge" fppMode values
#
# "master" mode was folded into Player in 2021 (sending sync packets became
# the MultiSyncEnabled setting) and "bridge" went with it.  The loaders in
# src/settings.cpp and www/config.php still map both to "player" in memory,
# but never write that back, so /media/settings can keep the old value for
# years -- visible to backup/restore and to anything reading the file directly.
#
# Idempotent: does nothing unless fppMode is one of the legacy values.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 148: migrate legacy fppMode"

MODE=$(getSetting fppMode)
case "${MODE}" in
    master)
        echo "  fppMode=master -> player, enabling MultiSync (matches the in-memory mapping)"
        setSetting fppMode player
        setSetting MultiSyncEnabled 1
        ;;
    bridge)
        echo "  fppMode=bridge -> player"
        setSetting fppMode player
        ;;
    *)
        echo "  fppMode='${MODE}' - nothing to do"
        ;;
esac

exit 0
