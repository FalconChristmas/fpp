#!/bin/sh

export BINDIR=$(dirname $(realpath $0))
export FPPDIR=$(dirname $(pwd))
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${FPPDIR}/lib:${FPPDIR}/external/RF24

sed -i -e "s/^restartFlag .*/restartFlag = \"0\"/" /home/fpp/media/settings

gdb --args ${BINDIR}/fppd -f -l stdout $*
