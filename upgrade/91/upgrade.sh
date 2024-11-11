#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ ! -f /etc/fpp/desktop ]; then
	rm -f /usr/lib/udev/rules.d/60-gpiochip4.rules
fi
