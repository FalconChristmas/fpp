#!/bin/bash

################################################################################
#
# By invoking this script with a custom UDEV rule, we are able to
# allow "bricked" counterfeit FTDI dongles in FPP.  We simply need
# the following in a udev rule:
#
# ACTION=="add", SUBSYSTEM=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="0000", RUN+="/opt/fpp/scripts/fix_pid_0000.sh"
#
# We added it to /etc/udev/rules.d/90-counterfeit-ftdi.rules starting
# with the v1.0 image
#
################################################################################

# Insert the FTDI module
modprobe ftdi_sio

# Force the driver to grab the 0'd PID
echo 0403 0000 > /sys/bus/usb-serial/drivers/ftdi_sio/new_id
