#!/bin/sh

echo $0 $@
cd /tmp

PRU=$1
shift

# clpru versions

ARMV=$(uname -m)

if [ "$ARMV" = "aarch64" ]; then
    CPPFLAG="-DAM62X"
    ENTRY="AM62x_PRU1.cmd"

    /usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG "/opt/fpp/src/non-gpl/FalconV5Support/FalconV5PRUListener.asm" > "/tmp/FalconV5Support_pru${PRU}.asm"
    clpru -v3 -o  $@ $CPPFLAG --endian=little --hardware_mac=on "/tmp/FalconV5Support_pru${PRU}.asm"
    clpru -v3 $CPPFLAG -z /opt/fpp/src/pru/$ENTRY -o "FalconV5Support_pru${PRU}.out" "FalconV5Support_pru${PRU}.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
else
    CPPFLAG="-DAM33XX"
    ENTRY="AM335x_PRU.cmd"
    /usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG "/opt/fpp/src/non-gpl/FalconV5Support/FalconV5PRUListener.asm" > "/tmp/FalconV5Support_pru${PRU}.asm"
    clpru -v3 -o  $@ $CPPFLAG "/tmp/FalconV5Support_pru${PRU}.asm"
    clpru -v3 -z --entry_point main /opt/fpp/src/pru/$ENTRY -o "FalconV5Support_pru${PRU}.out" "FalconV5Support_pru${PRU}.obj"
fi
#objdump -h FalconV5Support.out | grep .text |  awk '{print "dd if='FalconV5Support.out' of='FalconV5Support.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash
