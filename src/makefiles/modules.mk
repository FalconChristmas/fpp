
ifeq '$(ARCH)' 'Raspberry Pi'
#############################################################################
# RF24 library on the Pi
../external/RF24/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/RF24/librf24-bcm.so: ../external/RF24/.git $(PCH_FILE)
	@echo "Building RF24 library"
	@CC="ccache gcc" CXX="ccache g++" $(MAKE) -C ../external/RF24/
	@ln -s librf24-bcm.so.1.0 ../external/RF24/librf24-bcm.so.1
	@ln -s librf24-bcm.so.1 ../external/RF24/librf24-bcm.so

#############################################################################
# RGBMatrix library on the Pi
../external/rpi-rgb-led-matrix/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/rpi-rgb-led-matrix/lib/librgbmatrix.a: ../external/rpi-rgb-led-matrix/.git  $(PCH_FILE)
	@echo "Building rpi-rgb-led-matrix library"
	@if [ -e ../external/rpi-rgb-led-matrix/lib/librgbmatrix.so ]; then rm ../external/rpi-rgb-led-matrix/lib/librgbmatrix.so; fi
	@sed -i -e "s/-march=native//" ../external/rpi-rgb-led-matrix/lib/Makefile
	@CC="ccache gcc" CXX="ccache g++" $(MAKE) -C ../external/rpi-rgb-led-matrix/lib/ librgbmatrix.a
	@cd  ../external/rpi-rgb-led-matrix/lib/ && git checkout -- Makefile

#############################################################################
# libws2811 on the Pi
../external/rpi_ws281x/.git:
	@cd ../ && \
		git submodule init && \
		git submodule update

../external/rpi_ws281x/libws2811.a: ../external/rpi_ws281x/.git  $(PCH_FILE)
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

../external/spixels/lib/libspixels.a: ../external/spixels/.git  $(PCH_FILE)
	@echo "Building spixels library"
	@CC="ccache gcc" CXX="ccache g++" $(MAKE) -C ../external/spixels/lib/

clean::
	@if [ -e ../external/spixels/lib/libspixels.a ]; then $(MAKE) -C ../external/spixels/lib clean; fi
	@if [ -e ../external/RF24/.git ]; then $(MAKE) -C ../external/RF24 clean; fi
	@if [ -e ../external/rpi-rgb-led-matrix/.git ]; then $(MAKE) -C ../external/rpi-rgb-led-matrix clean; fi
	@if [ -e ../external/rpi_ws281x/libws2811.a ]; then rm ../external/rpi_ws281x/*.o ../external/rpi_ws281x/*.a 2> /dev/null; fi
    
endif
