#!/bin/bash
#####################################

# wiringPi doesn't work properly on Pi4 and is no longer open source, will need to start transitioning to pigpio
# pigpio in apt repo doesn't support pi4  yet either, need to build from source

FPPPLATFORM=$(cat /etc/fpp/platform 2> /dev/null)

if [ "${FPPPLATFORM}" == "Raspberry Pi" ]; then
    cd /opt
    rm -rf pigpio
    git clone https://github.com/joan2937/pigpio
    cd pigpio
    make prefix=/usr
    make install prefix=/usr
fi
