#!/bin/bash

if [ -d /opt/wiringPi ]; then
    cd /opt/wiringPi
    patch -p1 < /opt/fpp/upgrade/34/wiringPi.patch
    ./build
fi
