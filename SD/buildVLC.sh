#!/bin/sh


apt-get install -y flex bison pkg-config libasound2-dev

cd /opt
git clone git://git.videolan.org/vlc.git
cd vlc
git checkout d7c05336177a07e445d5c9021b5dba075cafc3a0

DISABLES=""
if [ -f /etc/fpp/desktop ]; then
    apt-get install libxcb-composite0-dev libxcb-xkb-dev libxcb-randr0-dev libxcb-shm0-dev
    DISABLES=""
else
    DISABLES="--disable-xcb"
fi


export LDFLAGS="-latomic"
./bootstrap
./configure --disable-lua --disable-a52 --disable-chromecast --disable-chromaprint  --disable-pulse --disable-jack --disable-dbus --disable-avahi --disable-qt $DISABLES --enable-run-as-root
make
make install
ldconfig
cd /opt
rm -rf vlc

