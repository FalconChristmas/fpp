#!/bin/sh
#
# asplashscreen install/upgrade correction script
#
# Update /etc/init.d/asplashscreen
#

cp ./asplashscreen /etc/init.d/asplashscreen
insserv /etc/init.d/asplashscreen
