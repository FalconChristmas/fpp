ifeq '$(ARCH)' 'Raspberry Pi'
CFLAGS += \
	-DPLATFORM_PI \
	-DUSEWIRINGPI

LIBS_fpp_so += \
	-lwiringPi \
	-lwiringPiDev

SUBMODULES += \
	external/RF24 \
	external/rpi-rgb-led-matrix \
	external/rpi_ws281x \
	external/spixels

LIBS_GPIO_ADDITIONS=-lwiringPi -lwiringPiDev
OBJECTS_GPIO_ADDITIONS=

#############################################################################
# RF24 library on the Pi
../external/RF24/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/RF24/librf24-bcm.so: ../external/RF24/.git
	@echo "Building RF24 library"
	@CC="ccache gcc" CXX="ccache g++" make -C ../external/RF24/
	@ln -s librf24-bcm.so.1.0 ../external/RF24/librf24-bcm.so.1
	@ln -s librf24-bcm.so.1 ../external/RF24/librf24-bcm.so

#############################################################################
# RGBMatrix library on the Pi
../external/rpi-rgb-led-matrix/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/rpi-rgb-led-matrix/lib/librgbmatrix.a: ../external/rpi-rgb-led-matrix/.git
	@echo "Building rpi-rgb-led-matrix library"
	@if [ -e ../external/rpi-rgb-led-matrix/lib/librgbmatrix.so ]; then rm ../external/rpi-rgb-led-matrix/lib/librgbmatrix.so; fi
	@CC="ccache gcc" CXX="ccache g++" make -C ../external/rpi-rgb-led-matrix/lib/ librgbmatrix.a

#############################################################################
# libws2811 on the Pi
../external/rpi_ws281x/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/rpi_ws281x/libws2811.a: ../external/rpi_ws281x/.git
	@echo "Building libws2811"
	@cd ../external/rpi_ws281x/ && \
		gcc -c -o rpihw.o rpihw.c && \
		gcc -c -o mailbox.o mailbox.c && \
		gcc -c -o dma.o dma.c && \
		gcc -c -o pcm.o pcm.c && \
		gcc -c -o pwm.o pwm.c && \
		gcc -c -o ws2811.o ws2811.c && \
		ar rcs libws2811.a rpihw.o mailbox.o dma.o pwm.o pcm.o ws2811.o

#############################################################################
# spixels library on the Pi
../external/spixels/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/spixels/lib/libspixels.a: ../external/spixels/.git
	@echo "Building spixels library"
	@CC="ccache gcc" CXX="ccache g++" make -C ../external/spixels/lib/

endif
