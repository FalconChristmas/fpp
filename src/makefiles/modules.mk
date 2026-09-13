
ifeq '$(ARCH)' 'Raspberry Pi'
#############################################################################
# RF24 library on the Pi
../external/RF24/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

# Flag selection via the compiler's actual target triple, NOT `uname -m`.
# Pi4/Pi5 running 32-bit Raspberry Pi OS boots a 64-bit kernel by default,
# so `uname -m` returns aarch64 even though the compiler is armhf. Picking
# -march=armv8-a for an armhf gcc then fails ("selected architecture lacks
# an FPU") because armv8-a implies the aarch64 FPU model.
PI_TARGET_TRIPLE := $(shell $(CXX) -dumpmachine 2>/dev/null)

ifneq ($(findstring aarch64,$(PI_TARGET_TRIPLE)),)
    RF24_CCFLAGS := -Ofast -march=armv8-a
else ifneq ($(findstring arm-linux-gnueabihf,$(PI_TARGET_TRIPLE)),)
    # armhf: armv6zk baseline keeps compatibility with every Pi ever shipped
    # (Pi 1 / Zero). Pi2+ supports more but armhf images are one-size-fits-all.
    # -marm: ARMv6 has no Thumb-2 and Thumb-1 + hard-float VFP is unsupported.
    # RF24 builds locally today (native, ARM-mode-default gcc) so it doesn't
    # strictly need this, but -marm keeps it safe if it is ever built with a
    # -mthumb-default toolchain (e.g. offloaded), matching pi.mk's main build.
    RF24_CCFLAGS := -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -marm -mtune=arm1176jzf-s
else ifneq ($(findstring arm,$(PI_TARGET_TRIPLE)),)
    # Generic arm (non-Debian triple) -- use armv7+neon.
    RF24_CCFLAGS := -Ofast -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7-a
else
    RF24_CCFLAGS := -Ofast
endif
RF24_CCFLAGS += -Wno-parentheses -Wno-unused-value -Wno-misleading-indentation -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

# RF24 driver selection — SPIDEV is the upstream default (nRF24/RF24#971) and
# fixes Pi5/RP1 + kernel 6.18 strict pinmux + /dev/mem issues. Keep soname
# librf24-bcm.so forever for plugin ABI compat (may change later). The
# FalconChristmas/RF24 fork is currently BCM-only (utility/SPIDEV missing);
# when it gains SPIDEV support this will automatically use it. Override with
# `make RF24_DRIVER=RPi` to force BCM for testing.
RF24_DRIVER ?= SPIDEV
# Detect whether the checked-out fork actually has SPIDEV sources
ifneq (,$(wildcard ../external/RF24/utility/SPIDEV/spi.cpp))
    RF24_HAS_SPIDEV := 1
else
    RF24_HAS_SPIDEV := 0
endif
ifeq ($(RF24_DRIVER),SPIDEV)
    ifeq ($(RF24_HAS_SPIDEV),1)
        RF24_ACTUAL_DRIVER := SPIDEV
        RF24_CCFLAGS += -DFPP_SPIDEV
        CXXFLAGS_channeloutput/SPInRF24L01.o += -DFPP_SPIDEV
    else
        $(warning RF24: SPIDEV requested but utility/SPIDEV missing in FalconChristmas/RF24 fork — falling back to RPi/BCM build. Sync fork with upstream nRF24/RF24 to enable SPIDEV.)
        RF24_ACTUAL_DRIVER := RPi
    endif
else
    RF24_ACTUAL_DRIVER := $(RF24_DRIVER)
endif

../external/RF24/librf24-bcm.so: ../external/RF24/.git $(PCH_FILE)
	@echo "Building RF24 library (driver: $(RF24_ACTUAL_DRIVER), requested: $(RF24_DRIVER))"
	@if [ "$(RF24_ACTUAL_DRIVER)" = "SPIDEV" ]; then \
		CC="ccache gcc" CXX="ccache g++" $(MAKE) -C ../external/RF24/ CCFLAGS="$(RF24_CCFLAGS)" DRIVER=SPIDEV LIB=librf24-bcm; \
	else \
		CC="ccache gcc" CXX="ccache g++" $(MAKE) -C ../external/RF24/ CCFLAGS="$(RF24_CCFLAGS)"; \
	fi
	@ln -sf librf24-bcm.so.1.0 ../external/RF24/librf24-bcm.so.1
	@ln -sf librf24-bcm.so.1 ../external/RF24/librf24-bcm.so

#############################################################################
# RGBMatrix library on the Pi
../external/rpi-rgb-led-matrix/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

# The library's config.mk defaults to `-march=native -mtune=native`, which is
# wrong for us twice over. An armhf gcc built as a cross compiler rejects
# `native` outright ("unrecognized -march target"), and a native gcc resolves it
# to the *build* host's CPU -- on a Pi5 that is cortex-a76, so an armhf build
# emits ARMv8 code (sdiv, movw/movt) that a Pi Zero cannot execute, and an
# aarch64 build is free to use ARMv8.2-only encodings absent on the Pi3/Pi4.
# FPP ships one image per word size, so pin the same baseline the rest of the
# build uses. Must be passed on the $(MAKE) command line: that overrides the
# `CPU_ARCH_FLAGS=` assignment inside config.mk, an environment variable would
# not. (The old `sed -i` of lib/Makefile was a no-op -- the flag lives in
# config.mk, so every build so far has quietly used -march=native.)
ifneq ($(findstring aarch64,$(PI_TARGET_TRIPLE)),)
    RGBMATRIX_CPU_ARCH_FLAGS := -march=armv8-a
else ifneq ($(findstring arm-linux-gnueabihf,$(PI_TARGET_TRIPLE)),)
    RGBMATRIX_CPU_ARCH_FLAGS := -march=armv6zk -mfpu=vfp -mfloat-abi=hard -marm -mtune=arm1176jzf-s
else ifneq ($(findstring arm,$(PI_TARGET_TRIPLE)),)
    RGBMATRIX_CPU_ARCH_FLAGS := -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard
else
    RGBMATRIX_CPU_ARCH_FLAGS :=
endif

../external/rpi-rgb-led-matrix/lib/librgbmatrix.a: ../external/rpi-rgb-led-matrix/.git  $(PCH_FILE)
	@echo "Building rpi-rgb-led-matrix library"
	@if [ -e ../external/rpi-rgb-led-matrix/lib/librgbmatrix.so ]; then rm ../external/rpi-rgb-led-matrix/lib/librgbmatrix.so; fi
	@CC="ccache gcc" CXX="ccache g++" $(MAKE) -C ../external/rpi-rgb-led-matrix/lib/ librgbmatrix.a DEFINES="-Wno-format" CPU_ARCH_FLAGS="$(RGBMATRIX_CPU_ARCH_FLAGS)"

#############################################################################
# spixels library on the Pi
../external/spixels/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/spixels/lib/libspixels.a: ../external/spixels/.git  $(PCH_FILE)
	@echo "Building spixels library"
	@CC="ccache gcc" CXX="ccache g++" $(MAKE) -C ../external/spixels/lib/ CXXFLAGS="-fPIC -Wall -O3 -I../include -I. -Wno-mismatched-new-delete"

clean::
	@if [ -e ../external/spixels/lib/libspixels.a ]; then $(MAKE) -C ../external/spixels/lib clean; fi
	@if [ -e ../external/RF24/.git ]; then $(MAKE) -C ../external/RF24 clean; fi
	@if [ -e ../external/rpi-rgb-led-matrix/.git ]; then $(MAKE) -C ../external/rpi-rgb-led-matrix clean; fi
    
endif
