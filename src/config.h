#pragma once

#ifdef PLATFORM_OSX

#define SHLIB_EXT ".dylib"

extern void MacOSSetVolume(int v);
extern int MacOSGetVolume();
#define USE_FRAMEBUFFER_SOCKET

#else

#define HAS_SPI
#define HAS_I2C
#define HAS_GPIOD

#define SHLIB_EXT ".so"

#if defined(PLATFORM_RPI) || defined(PLATFORM_BBB) || defined(PLATFORM_BB64)
#define HAS_CAPEUTILS
#endif

#endif
