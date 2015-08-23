#!/bin/bash
#
# Updates to bring master users in line with needs of FPP v1.5 SD images
# - Install packages for OLA for real this time
#

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

export DEBIAN_FRONTEND=noninteractive

sudo apt-get update

case "${FPPPLATFORM}" in
    'Raspberry Pi')
		apt-get update
		apt-get -y --force-yes install ola ola-rdm-tests ola-conf-plugins ola-dev libprotobuf-dev
	;;
esac

sudo apt-get -y clean

