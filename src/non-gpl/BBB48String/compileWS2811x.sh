#!/bin/sh

echo $0 $@
cd /tmp




ARMV=$(uname -m)

if [ "$ARMV" = "aarch64" ]; then
    #for now, we only support PRUSS, need a better way to figure out the types, core speeds, etc...
    CPPFLAG="-DAM62X"
    ENTRY="AM62x_PRU1.cmd"

    # clpru versions
    /usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG /opt/fpp/src/non-gpl/BBB48String/FalconWS281x.asm > /tmp/FalconWS281x.asm
    clpru -v3 -o  $@ $CPPFLAG --endian=little --hardware_mac=on /tmp/FalconWS281x.asm
    echo clpru -v3 -$CPPFLAG -z  /opt/fpp/src/pru/$ENTRY -o FalconWS281x.out FalconWS281x.obj
    clpru -v3 $CPPFLAG -z /opt/fpp/src/pru/$ENTRY -o FalconWS281x.out FalconWS281x.obj -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
    #objdump -h FalconWS281x.out | grep .text |  awk '{print "dd if='FalconWS281x.out' of='FalconWS281x.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

else
    # clpru versions
    CPPFLAG="-DAM33XX"
    /usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG /opt/fpp/src/non-gpl/BBB48String/FalconWS281x.asm > /tmp/FalconWS281x.asm
    clpru -v3 -o $@ $CPPFLAG /tmp/FalconWS281x.asm
    clpru -v3 -z --entry_point main /opt/fpp/src/pru/AM335x_PRU.cmd -o FalconWS281x.out FalconWS281x.obj
    #objdump -h FalconWS281x.out | grep .text |  awk '{print "dd if='FalconWS281x.out' of='FalconWS281x.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash
fi


