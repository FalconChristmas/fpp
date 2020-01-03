#!/bin/sh

export BINDIR=$(dirname $(realpath $0))
export FPPDIR=$(dirname $(pwd))
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FPPDIR}/lib:${FPPDIR}/external/RF24

${BINDIR}/fppd -f $*
