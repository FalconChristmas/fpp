#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

apt-get update
apt-get install -y php-bcmath
apt-get clean

# Need to reboot so that php will pickup the new lib
setSetting rebootFlag 1
