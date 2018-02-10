#!/bin/bash
#
# Install libmicrohttpd-dev and libhttpserver for fppd REST API
#

apt-get -y update
apt-get -y install libmicrohttpd-dev

(cd /opt/ && git clone https://github.com/etr/libhttpserver && cd libhttpserver && git checkout bd08772 && ./bootstrap && mkdir build && cd build && CXXFLAGS=-std=c++98 ../configure --prefix=/usr && make && make install && cd /opt/ && rm -rf /opt/libhttpserver)

