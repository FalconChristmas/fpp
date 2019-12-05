#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

if [ "${FPPPLATFORM}" == "Raspberry Pi" ]; then
    # disable the swap file, wears out the SD card faster
    systemctl disable dphys-swapfile
    
    #unset the old gpu_mem setting
    sed -i -e "s/gpu_mem/#gpu_mem/" /boot/config.txt

    #set new value based on model
    echo "# GPU memory set to 128 to deal with error in omxplayer with hi-def videos" >> /boot/config.txt
    echo "[pi4]" >> /boot/config.txt
    echo "gpu_mem=128" >> /boot/config.txt
    echo "[pi3]" >> /boot/config.txt
    echo "gpu_mem=128" >> /boot/config.txt
    echo "[pi0]" >> /boot/config.txt
    echo "gpu_mem=64" >> /boot/config.txt
    echo "[pi1]" >> /boot/config.txt
    echo "gpu_mem=64" >> /boot/config.txt
    echo "[pi2]" >> /boot/config.txt
    echo "gpu_mem=64" >> /boot/config.txt
    echo >> /boot/config.txt
        
    echo "A reboot will be required to get the new Apache config working working"
fi

