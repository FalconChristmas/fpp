#!/bin/sh
###############################################################################
# Convert /etc/rc.local to use the new scripts in /home/pi/fpp/scripts

echo "Patching /etc/rc.local to enable use of new fpp scripts"
patch /etc/rc.local rc.local.diff

