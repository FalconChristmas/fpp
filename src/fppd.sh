#!/bin/sh

export FPPDIR=$(dirname $(pwd))
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FPPDIR}/lib:${FPPDIR}/external/RF24

./fppd $*
