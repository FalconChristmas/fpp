#!/bin/bash
cd /opt
git clone https://github.com/ccache/ccache
cd ccache
git checkout v4.6.1

apt-get install -y cmake

cmake -DCMAKE_BUILD_TYPE=Release -DREDIS_STORAGE_BACKEND=OFF -DCMAKE_INSTALL_PREFIX=/usr -DENABLE_TESTING=OFF .
make
make install

apt-get remove -y --autoremove cmake
cd /opt
rm -rf ccache
