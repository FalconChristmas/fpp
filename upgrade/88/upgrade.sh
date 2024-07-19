#!/bin/bash
#####################################
BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common


# On all linux devices, we need libkms++
apt-get update
apt-get install libkms++-dev
apt-get clean

if [ "${FPPPLATFORM}" == "Raspberry Pi" ]; then
    # on the Pi's we need to update the config.txt to make sure it has the vc4-kms-v3d
    # dtoverlay so that KMS will work
    echo "[ALL]" >> /boot/firmware/config.txt
    echo "dtoverlay=vc4-kms-v3d" >> /boot/firmware/config.txt
    echo "" >> /boot/firmware/config.txt

    # Need to update the cmdline to remove the old sound compatibility settings
    # and also to turn on the headphone jack
    sed -i 's/snd_bcm2835\.\([a-z0-9_]\)*=1//g' /boot/firmware/cmdline.txt
    sed -i 's/$/ snd_bcm2835.enable_headphones=1/' /boot/firmware/cmdline.txt
    sed -i -e 's/  */ /g' /boot/firmware/cmdline.txt

    # Remove any old DPI pixel configuration
    sed -i -e '/^include dpipixels.txt/d' /boot/firmware/config.txt
    rm -f /boot/firmware/dpipixels.txt.new

    # Set the reboot flag so the config/cmdline changes will be picked up
    setSetting rebootFlag 1
fi