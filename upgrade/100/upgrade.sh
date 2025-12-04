#!/bin/bash
#####################################
BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ "${FPPPLATFORM}" == "BeagleBone 64" ]; then
    cd /opt/fpp/capes/drivers/bb64
    make install
    make clean
    setSetting rebootFlag 1
fi
