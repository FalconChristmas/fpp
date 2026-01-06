#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# Install updated networkd-dispatcher script for NTP sync on network routable
# This script forces ntpd to sync with -g flag when network becomes available
# Critical for systems without RTC (GitHub issue #2535)
mkdir -p /etc/networkd-dispatcher/routable.d
cp /opt/fpp/etc/networkd-dispatcher/routable.d/ntpd /etc/networkd-dispatcher/routable.d/ntpd
chmod +x /etc/networkd-dispatcher/routable.d/ntpd

