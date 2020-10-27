#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ "${FPPPLATFORM}" == "Beaglebone Black" ]; then
    cp /opt/fpp/upgrade/64/bbb-fpp-reserve-memory.dtbo /lib/firmware/bbb-fpp-reserve-memory.dtbo
    setSetting rebootFlag 1
fi

