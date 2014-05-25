=====================================================================
Instructions on building the rtc-pcf2127a.ko kernel module for the
RasClock clock module for the Raspberry Pi.
=====================================================================

Setup a cross-compiler using the instructions provided at the
following link.  If you install the cross-compiler in
~/src/pi/tools-master, you can use the commands below directly.
If you install in another location, you will need to adjust some
of the commands below to reflect the correct location.

	http://elinux.org/RPi_Kernel_Compilation#Perform_the_compilation

Add the cross-compiler's bin directory to your path (bash format given here):

	export PATH=${PATH}:~/src/pi/tools-master/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/bin/

Clone a copy of the Linux kernel source code:

	mkdir -p ~/src/pi
	cd ~/src/pi/
	git clone https://github.com/raspberrypi/linux.git

Checkout the branch for the 3.10.y kernel which is the current verion on the Pi:

	cd ./linux
	git checkout -b rpi-3.10.y origin/rpi-3.10.y

Get a copy of the running kernel config off an the Pi:

	ssh pi@fpp "zcat /proc/config.gz" > .config

Make the kernel config and build the kernel:

	make ARCH=arm CROSS_COMPILE=arm-bcm2708hardfp-linux-gnueabi- oldconfig
	make ARCH=arm CROSS_COMPILE=arm-bcm2708hardfp-linux-gnueabi-

Build the rasclock module by changing to the directory containing this
README.txt file and run "make".  This will download the rasclock source
and compile the kernel module leaving you with a 'rtc-pcf2127a.ko' kernel
module in the same directory.  If you installed the kernel source in a
different directrory other than ~/src/pi/linux, then you will need to edit
the Makefile to provide the correct location in the LINUX_DIR variable.

	make

Copy the kernel module to the Pi:

	scp rtc-pcf2127a.ko pi@fpp:./

ssh to the Pi and copy the module to the lib/modules directory.  This example
shows the 3.10.38+ kernel directory which was the latest at the time of the
writing of this document.

	ssh pi@fpp
	sudo cp rtc-pcf2127a.ko /lib/modules/3.10.38+/kernel/drivers/rtc/

Test load the module while still logged into the Pi:

	sudo depmod -a
	sudo modprobe rtc-pcf2127a

If all is well, log out of the Pi and clean the temporary files from the
rasclock build directory:

	(on Pi)
	exit
	(on build host)
	make clean

