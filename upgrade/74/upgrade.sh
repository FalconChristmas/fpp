#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

mkdir /var/log/exim4
chown Debian-exim /var/log/exim4
chgrp Debian-exim /var/log/exim4

apt-get -y install mailutils

