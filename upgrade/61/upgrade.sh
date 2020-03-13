#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

cp /opt/fpp/etc/systemd/fppd.service /lib/systemd/system

systemctl daemon-reload
systemctl reenable fppd
setSetting rebootFlag 1
