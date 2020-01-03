#!/bin/bash
#####################################
# copy the oled service definition ot systemd and turn the service
# on a boot.  We won't set the reboot flag as the default is disabled
# and once the user selects an OLED type, it will set the reboot flag

cp -f ${FPPDIR}/etc/systemd/fppoled.service /lib/systemd/system
systemctl enable fppoled

