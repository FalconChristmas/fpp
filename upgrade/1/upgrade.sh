#!/bin/sh
###############################################################################
# Convert /etc/rc.local to use the new scripts in fpp/scripts

if [ "x${FPPVERSION}" = "x10" ]
then
	echo "Updating /etc/rc.local to enable use of new fpp scripts dir"
	cp /etc/rc.local /etc/rc.local.orig
	cp rc.local /etc/rc.local
fi

