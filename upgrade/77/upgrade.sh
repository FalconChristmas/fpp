#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

apt-get update
apt-get install -y libusb-1.0-0-dev
apt-get clean

