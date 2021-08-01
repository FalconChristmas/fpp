#!/bin/bash

# Running from vscode likely means debug builds so we'll
# increase the size of the ccache to account for the larger .o files
ccache -M 500M

#determine the number of cores so we can set an appropriate -j flag to make
CPUS=$(grep "^processor" /proc/cpuinfo | wc -l)

cd /opt/fpp/src
make -j $CPUS  $@

