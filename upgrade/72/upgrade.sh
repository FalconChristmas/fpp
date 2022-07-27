#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ "${FPPPLATFORM}" == "BeagleBone Black" ]; then
    rm -rf /etc/udev/rules.d/*SoftAp*
else
    echo "No need to update for platform ${FPPPLATFORM}"
fi

