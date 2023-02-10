#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

apt-get update
apt-get install -y php-bcmath
apt-get clean

