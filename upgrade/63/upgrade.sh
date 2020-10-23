#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ "${FPPPLATFORM}" == "Raspberry Pi" ]; then
    HDMIFORCE=$(tail -n 10 /boot/config.txt | grep hdmi_force)
    echo "" >> /boot/config.txt
    echo "[all]" >> /boot/config.txt
    if [ "${HDMIFORCE}" != "" ]; then
        sed -i '/hdmi_force_hotplug/d' /boot/config.txt;
        echo $HDMIFORCE >> /boot/config.txt
    fi
fi

