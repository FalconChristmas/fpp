#!/bin/sh

echo $0 $@
cd /tmp

PRU=$1
shift


ARMV=$(uname -m)

if [ "$ARMV" = "aarch64" ]; then
    CPPFLAG="-DAM62X"
    ENTRY="AM62x_PRU1.cmd"

    /usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString_pru${PRU}.asm"
    clpru -v3 -o  $@ $CPPFLAG --endian=little --hardware_mac=on "/tmp/BBShiftString_pru${PRU}.asm"
    clpru -v3 $CPPFLAG -z /opt/fpp/src/pru/$ENTRY -o "BBShiftString_pru${PRU}.out" "BBShiftString_pru${PRU}.obj" -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
else
    CPPFLAG="-DAM33XX"
    /usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString_pru${PRU}.asm"
    clpru -v3 -o  $@ $CPPFLAG "/tmp/BBShiftString_pru${PRU}.asm"
    clpru -v3 -z --entry_point main /opt/fpp/src/pru/AM335x_PRU.cmd -o "BBShiftString_pru${PRU}.out" "BBShiftString_pru${PRU}.obj"
fi

