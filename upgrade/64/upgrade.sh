#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ "${FPPPLATFORM}" == "BeagleBone Black" ]; then
    echo "Setting up new reserved memory  block"
    cp -f /opt/fpp/upgrade/64/bbb-fpp-reserve-memory.dtbo /lib/firmware/bbb-fpp-reserve-memory.dtbo
    setSetting rebootFlag 1
else
    echo "No need to update for platform ${FPPPLATFORM}"
fi

