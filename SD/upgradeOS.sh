#!/bin/bash

if [ "x$1" == "x" ]
then
    echo "USAGE: $0 FILENAME_OF_FPPOS_IMAGE.fppos"
    exit 0
fi

IMAGE=$1
if [ ! -f "/home/fpp/media/upload/${IMAGE}" ]
then
    echo "ERROR: /home/fpp/media/upload/${IMAGE} does not exist."
    exit 0
fi

DIR=$(dirname $0)
echo "Making temp copy of upgrade script."
cp ${DIR}/upgradeOS-part1.sh /home/fpp/media/tmp/upgradeOS-part1.sh

echo "Upgrading FPP OS using /home/fpp/media/upload/${IMAGE}"
echo ========================================================================

exec sudo /home/fpp/media/tmp/upgradeOS-part1.sh /home/fpp/media/upload/${IMAGE}

rm /home/fpp/media/tmp/upgradeOS-part1.sh

echo ========================================================================
echo "You can reboot from the command line by running the following command:"
echo "sudo shutdown -r now"
