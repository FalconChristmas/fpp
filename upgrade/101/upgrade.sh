#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# Install libsystemd-dev if not already present (needed for sd_notify support)
if ! dpkg -l | grep -q libsystemd-dev; then
    echo "Installing libsystemd-dev for systemd notify support"
    apt-get update
    apt-get install -y libsystemd-dev
fi

# Update systemd service files with notify support and reduced timeouts
cp /opt/fpp/etc/systemd/fppinit.service /lib/systemd/system
cp /opt/fpp/etc/systemd/fpp_postnetwork.service /lib/systemd/system
cp /opt/fpp/etc/systemd/fppd.service /lib/systemd/system

systemctl daemon-reload
