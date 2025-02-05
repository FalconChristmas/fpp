#!/bin/sh

echo $0 $@
cd /tmp
/bin/rm -f /opt/fpp/lib/FalconMatrix.*
/bin/rm -f /opt/fpp/lib/FalconMatrixPRUCpy.*

ARMV=$(uname -m)
if [ "$ARMV" = "aarch64" ]; then
    #for now, we only support PRUSS, need a better way to figure out the types, core speeds, etc...
    CPPFLAG="-DAM62X"
    ENTRY="AM62x_PRU0.cmd"

    # clpru versions
    /usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG /opt/fpp/src/pru/FalconMatrix.asm > /tmp/FalconMatrix.asm
    clpru -v3 -o  $@ $CPPFLAG --endian=little --hardware_mac=on /tmp/FalconMatrix.asm
    clpru -v3 $CPPFLAG -z /opt/fpp/src/pru/$ENTRY -o FalconMatrix.out FalconMatrix.obj -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a

    ENTRY="AM62x_PRU1.cmd"
    /usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG /opt/fpp/src/pru/FalconMatrixPRUCpy.asm > /tmp/FalconMatrixPRUCpy.asm
    clpru -v3 -o $@ $CPPFLAG --endian=little --hardware_mac=on /tmp/FalconMatrixPRUCpy.asm
    clpru -v3 -z /opt/fpp/src/pru/$ENTRY -o FalconMatrixPRUCpy.out FalconMatrixPRUCpy.obj -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
else
    # clpru versions
    CPPFLAG="-DAM33XX"

    # clpru versions
    /usr/bin/cpp -P $@ $CPPFLAG /opt/fpp/src/pru/FalconMatrix.asm > /tmp/FalconMatrix.asm
    clpru -v3 -o $@ $CPPFLAG /tmp/FalconMatrix.asm
    clpru -v3 -z --entry_point main /opt/fpp/src/pru/AM335x_PRU.cmd -o FalconMatrix.out FalconMatrix.obj
    #objdump -h FalconMatrix.out | grep .text |  awk '{print "dd if='FalconMatrix.out' of='FalconMatrix.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

    /usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG /opt/fpp/src/pru/FalconMatrixPRUCpy.asm > /tmp/FalconMatrixPRUCpy.asm
    clpru -v3 -o $@ $CPPFLAG /tmp/FalconMatrixPRUCpy.asm
    clpru -v3 -z --entry_point main /opt/fpp/src/pru/AM335x_PRU.cmd -o FalconMatrixPRUCpy.out FalconMatrixPRUCpy.obj
    #objdump -h FalconMatrixPRUCpy.out | grep .text |  awk '{print "dd if='FalconMatrixPRUCpy.out' of='FalconMatrixPRUCpy.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

fi


