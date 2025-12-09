#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# Update systemd service timeout values to support boot delays up to 5 minutes
cp /opt/fpp/etc/systemd/fppinit.service /lib/systemd/system
cp /opt/fpp/etc/systemd/fpp_postnetwork.service /lib/systemd/system
cp /opt/fpp/etc/systemd/fppd.service /lib/systemd/system

systemctl daemon-reload
