#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/functions

CPUS=$(ComputeMakeParallelism)
FPPPLATFORM=$(cat /etc/fpp/platform)

if [ "$FPPPLATFORM" == "Raspberry Pi" -a ! -f "/boot/firmware/overlays/vc4-kms-dpi-fpp.dtbo" ]; then
    echo "FPP - Creating FPP DPI Overlay"
    cd /opt/fpp/capes/drivers/pi
    make -j ${CPUS}
    make install
    make clean
    cd ~
fi


# Set the reboot flag so the config/cmdline changes will be picked up
. /opt/fpp/scripts/common
setSetting rebootFlag 1

