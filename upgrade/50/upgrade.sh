#!/bin/bash
#####################################
# Upgrade uio_pruss to 3M to allow more memory for 10bit panels

echo "# allocate 3M instead of the default 256K" > /etc/modprobe.d/uio_pruss.conf
echo "options uio_pruss extram_pool_sz=3145728" >> /etc/modprobe.d/uio_pruss.conf

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common
setSetting rebootFlag 1
echo "A reboot will be required to get the new UIO PRUSS memory pool configured"

