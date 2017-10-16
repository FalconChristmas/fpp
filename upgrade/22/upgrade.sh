#!/bin/bash
#
# Fix potential ping issue found by Pat
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

case "${FPPPLATFORM}" in
	'BeagleBone Black')
		chmod u+s /bin/ping
	;;
esac


