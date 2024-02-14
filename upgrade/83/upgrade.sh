#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# remove fppcapedetect, now part of fppinit
systemctl disable fppcapedetect
rm -f /lib/systemd/system/fppcapedetect.service

cp /opt/fpp/etc/systemd/fpp* /lib/systemd/system

systemctl daemon-reload
systemctl reenable fppinit
systemctl reenable fppoled
systemctl reenable fpprtc
systemctl reenable fpp_postnetwork
systemctl reenable fppd
