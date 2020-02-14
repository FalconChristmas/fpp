#!/bin/sh

cd /opt
git clone git://git.videolan.org/vlc.git
cd vlc
git checkout d7c05336177a07e445d5c9021b5dba075cafc3a0


apt-get install -y flex bison
export LDFLAGS="-latomic"
./bootstrap
./configure --disable-lua --disable-a52 --disable-xcb --disable-chromecast --disable-chromaprint --disable-qt --disable-pulse --disable-jack --disable-dbus --disable-avahi  --enable-run-as-root 
make
make install
ldconfig
cd /opt
rm -rf vlc

