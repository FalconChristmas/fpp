#!/bin/bash

cd /opt
git clone https://code.videolan.org/videolan/vlc.git
cd vlc
# this is latest master as of 01/25/2022
git checkout d2e9f04ee4ba121095ae16e98231da8c3431c80e
# this commit is causing a segfault, logged as:
# https://code.videolan.org/videolan/vlc/-/issues/25790
git revert --no-edit fbb54457118b61f56f4d2c12c5e7a170c04ea48d


cat > patch.diff <<-EOF
diff --git a/configure.ac b/configure.ac
index e8af70f704..27c87175d7 100644
--- a/configure.ac
+++ b/configure.ac
@@ -70,30 +70,6 @@ AC_C_INLINE
 AC_C_RESTRICT
 AX_CXX_COMPILE_STDCXX_14([noext], [mandatory])

-dnl Check the compiler supports atomics in C
-AC_MSG_CHECKING([C atomics])
-VLC_SAVE_FLAGS
-ATOMIC_LIB=""
-AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <stdatomic.h>],[
- atomic_uintmax_t test;
- atomic_init(&test, 0);
- atomic_fetch_add_explicit(&test, 2u, memory_order_relaxed);
-])], [AC_MSG_RESULT([built-in])], [
-  LDFLAGS="\$LDFLAGS -latomic"
-  AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <stdatomic.h>],[
-atomic_uintmax_t test;
-atomic_init(&test, 0);
-atomic_fetch_add_explicit(&test, 2u, memory_order_relaxed);
-])],[
-    AC_MSG_RESULT([using -latomic])
-    ATOMIC_LIB="-latomic"
-  ],[
-  AC_MSG_ERROR([VLC requires atomics support])
-  ])
-])
-VLC_RESTORE_FLAGS
-AC_SUBST([ATOMIC_LIB])
-
 dnl Extend the --help string at the current spot.
 AC_DEFUN([EXTEND_HELP_STRING], [m4_divert_once([HELP_ENABLE], [\$1])])
EOF

patch -p 1 < patch.diff

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
    apt-get install libxcb-composite0-dev libxcb-xkb-dev libxcb-randr0-dev libxcb-shm0-dev
    DISABLES=""
else
    DISABLES="--disable-xcb"
fi

CPUS=$(grep "^processor" /proc/cpuinfo | wc -l)
./bootstrap
export LDFLAGS="-latomic"
./configure --disable-gles2 --disable-lua --disable-a52 --disable-chromecast --disable-chromaprint  --disable-pulse --disable-jack --disable-dbus --disable-avahi --disable-qt $DISABLES --enable-run-as-root
make -j ${CPUS}
make install
ldconfig
cd /opt
rm -rf vlc

