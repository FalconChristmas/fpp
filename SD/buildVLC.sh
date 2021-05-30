#!/bin/sh


apt-get install -y flex bison pkg-config libasound2-dev

cd /opt
git clone https://code.videolan.org/videolan/vlc.git
cd vlc
# this is latest master as of 05/30/2021
git checkout 3d07a3d41fed1c4c8de779272e54b13a4e53c8c5
# this commit is causing a segfault, logged as:
# https://code.videolan.org/videolan/vlc/-/issues/25790
git revert fbb54457118b61f56f4d2c12c5e7a170c04ea48d

# mesa-common-dev ?

DISABLES=""
if [ -f /etc/fpp/desktop ]; then
    apt-get install libxcb-composite0-dev libxcb-xkb-dev libxcb-randr0-dev libxcb-shm0-dev
    DISABLES=""
else
    DISABLES="--disable-xcb"
fi

export LDFLAGS="-latomic"
./bootstrap
./configure --disable-gles2 --disable-lua --disable-a52 --disable-chromecast --disable-chromaprint  --disable-pulse --disable-jack --disable-dbus --disable-avahi --disable-qt $DISABLES --enable-run-as-root
make
make install
ldconfig
cd /opt
rm -rf vlc

