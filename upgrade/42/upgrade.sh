#!/bin/bash
#####################################
# copy the oled service definition ot systemd and turn the service
# on a boot.  We won't set the reboot flag as the default is disabled
# and once the user selects an OLED type, it will set the reboot flag

cp -f /opt/fpp/etc/systemd/*.service /lib/systemd/system
systemctl disable fppoled
systemctl disable fppinit
systemctl enable fppoled
systemctl enable fpprtc
systemctl enable fppcapedetect
systemctl enable fppinit
