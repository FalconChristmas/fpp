#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# need to re-copy the boot scripts
cp /opt/fpp/etc/systemd/fpp* /lib/systemd/system

systemctl daemon-reload
