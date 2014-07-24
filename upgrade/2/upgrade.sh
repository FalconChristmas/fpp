#!/bin/sh
#
# fppLCD install/upgrade script
#
# Update package database
#


sudo apt-get -q -y update

#
# Install required packages
#

sudo apt-get -q -y --reinstall install python-smbus python-daemon i2c-tools

#
# install required modules
#

sudo echo "i2c-bcm2708" >> /etc/modules
sudo echo "i2c-dev" >> /etc/modules

#
# load modules into rpi kernel
#

sudo modprobe i2c-bcm2708
sudo modprobe i2c-dev

#
# script finished, upon activation in the GUI - fppLCD will start up
#