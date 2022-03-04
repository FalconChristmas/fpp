#!/bin/bash

# Running from vscode likely means debug builds so we'll
# increase the size of the ccache to account for the larger .o files
if [ -f /usr/bin/ccache ]; then
    ccache -M 500M
fi

#determine the number of cores so we can set an appropriate -j flag to make
if [ -f /proc/cpuinfo  ]; then
    CPUS=$(grep "^processor" /proc/cpuinfo | wc -l)
else
    CPUS=8
fi

cd /opt/fpp/src
make -j $CPUS  $@

