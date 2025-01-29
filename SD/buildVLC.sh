#!/bin/bash

ARMV=$(uname -m)
case "${ARMV}" in
    "aarch64" | "arm64" | "arm" | "armv7l" )
        # On Arm, we'll install the vlc from the repository so we'll get whatever acceleration
        # is provided by the arm platform.  Most is not pushed upstream.
        apt-get install -y vlc libvlc-dev
        apt-get clean
    ;;
    * )
        cd /opt
        git clone https://code.videolan.org/videolan/vlc.git
        cd vlc
        # this is latest master as of 01/28/2025
        git checkout 94f273566e2d12987f37bd9e0a601b49be86ad17

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
        ./configure --disable-gles2 --disable-lua --disable-a52 --disable-chromecast --disable-chromaprint  --disable-pulse --disable-jack --disable-dbus --disable-avahi --disable-qt $DISABLES --enable-run-as-root --enable-libdrm
        make -j ${CPUS}
        make install
        ldconfig
        cd /opt
        rm -rf vlc
    ;;
esac


