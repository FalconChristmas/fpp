#!/bin/bash

cd /opt
git clone https://code.videolan.org/videolan/vlc.git
cd vlc
# this is latest master as of 07/25/2022
git checkout 61556384147016a0a43351af9c13f1a898c85a89
# this commit is causing a segfault, logged as:
# https://code.videolan.org/videolan/vlc/-/issues/25790
git revert --no-edit fbb54457118b61f56f4d2c12c5e7a170c04ea48d

# make sure symlink to libGL exists.  On Beagles and docker, it won't unless
# a BUNCH of dev packages are installed which we'd like to avoid
LIBDIR=$(pkg-config --variable=libdir zlib)
if [ -f ${LIBDIR}/libGL.so.1 ]; then
    if [ ! -f ${LIBDIR}/libGL.so ]; then
        ln -s ${LIBDIR}/libGL.so.1 ${LIBDIR}/libGL.so
    fi
fi

DISABLES=""
if [ -f /etc/fpp/desktop ]; then
    apt-get install -y libxcb-composite0-dev libxcb-xkb-dev libxcb-randr0-dev libxcb-shm0-dev
    DISABLES=""
else
    DISABLES="--disable-xcb"
fi

CPUS=$(grep "^processor" /proc/cpuinfo | wc -l)
MEMORY=$(grep MemTot /proc/meminfo | awk '{print $2}')
if [[ ${MEMORY} -lt 512000 ]]; then
    CPUS=1
fi

export LDFLAGS="-latomic"
./bootstrap
export LDFLAGS="-latomic"
./configure --disable-gles2 --disable-lua --disable-a52 --disable-chromecast --disable-chromaprint  --disable-pulse --disable-jack --disable-dbus --disable-avahi --disable-qt $DISABLES --enable-run-as-root
make -j ${CPUS}
make install
ldconfig
cd /opt
rm -rf vlc

cat > /usr/local/include/vlc/fpp-vlc-build.h << EOF
#pragma once
#define FPP_VLC_DATE 20220725
#define LIBVLC_MEDIA_NEWPATH(a, b) libvlc_media_new_path(b)
#define LIBVLC_MEDIAPLAYER_NEW_FROM_MEDIA(a, b) libvlc_media_player_new_from_media(a, b)
EOF

