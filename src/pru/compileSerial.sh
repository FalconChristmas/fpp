#!/bin/sh

echo $0 $@
cd /tmp
/bin/rm -f /opt/fpp/lib/FalconSerial.*


ARMV=$(uname -m)
if [ "$ARMV" = "aarch64" ]; then
    #for now, we only support PRUSS, need a better way to figure out the types, core speeds, etc...
    CPPFLAG="-DAM62X"
    ENTRY="AM62x_PRU1.cmd"

	# clpru versions
	/usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG /opt/fpp/src/pru/FalconSerial.asm > /tmp/FalconSerial.asm
	clpru -v3 -o $@ $CPPFLAG --endian=little --hardware_mac=on /tmp/FalconSerial.asm
	clpru -v3 $CPPFLAG -z /opt/fpp/src/pru/$ENTRY -o FalconSerial.out FalconSerial.obj -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
	#objdump -h FalconSerial.out | grep .text |  awk '{print "dd if='FalconSerial.out' of='FalconSerial.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

else
    # clpru versions
    CPPFLAG="-DAM33XX"
	
	# clpru versions
	/usr/bin/cpp -P $@ $CPPFLAG /opt/fpp/src/pru/FalconSerial.asm > /tmp/FalconSerial.asm
	clpru -v3 -o $@ $CPPFLAG /tmp/FalconSerial.asm
	clpru -v3 -z --entry_point main  /opt/fpp/src/pru/AM335x_PRU.cmd  -o FalconSerial.out FalconSerial.obj
	#objdump -h FalconSerial.out | grep .text |  awk '{print "dd if='FalconSerial.out' of='FalconSerial.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

fi