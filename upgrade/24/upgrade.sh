#!/bin/bash
#
# Install libmicrohttpd and libhttpserver
#

apt-get -y upgrade
apt-get -y install libmicrohttpd-dev libmicrohttpd10

cd /opt/ \
	&& git clone https://github.com/etr/libhttpserver \
	&& cd libhttpserver \
	&& git checkout 02df5e7 \
	&& ./bootstrap \
	&& mkdir build \
	&& cd build \
	&& ../configure --prefix=/usr \
	&& make \
	&& make install \
	&& cd /opt/ \
	&& rm -rf /opt/libhttpserver

