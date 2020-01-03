#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# Disabling Apache proxy_https module
echo "Disabling Apache Header modules"
a2dismod proxy_http2

echo "Configuring ccache to use /tmp"
ccache --set-config=temporary_dir=/tmp

setSetting rebootFlag 1
echo "A reboot will be required to restart Apache"
