#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ "${FPPPLATFORM}" == "BeagleBone Black" ]; then
    echo "Setting up new audio driver clocks"
    cp -f /opt/fpp/upgrade/65/BBB-AUDIO-PCM5012A-00A0.dtbo /lib/firmware/BBB-AUDIO-PCM5012A-00A0.dtbo
    setSetting rebootFlag 1
else
    echo "No need to update for platform ${FPPPLATFORM}"
fi

