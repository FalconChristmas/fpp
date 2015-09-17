#!/bin/bash
#
# Revert earlier change, doesn't appear to be supported with this module
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

case "${FPPPLATFORM}" in
	'BeagleBone Black')
		rm /etc/modprobe.d/rtl8192cu.conf
	;;
esac


