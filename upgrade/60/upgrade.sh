#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Installing libhttpserver 0.17.5"
cd /opt/
git clone https://github.com/etr/libhttpserver
cd libhttpserver
git checkout 0.17.5
./bootstrap
mkdir build
cd build
../configure --prefix=/usr
make
make install
cd /opt/
rm -rf /opt/libhttpserver
