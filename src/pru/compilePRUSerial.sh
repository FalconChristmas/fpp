#!/bin/sh

echo $0 $@
cd /tmp

# a failed build must not leave the previous program behind for the driver to load
rm -f /tmp/PRUSerial.obj /tmp/PRUSerial.out

# AM62x only: on the AM335x boards every pin this is needed for has a UART
CPPFLAG="-DAM62X"
ENTRY="AM62x_PRU0.cmd"
for a in "$@"; do
    if [ "$a" = "-DRUNNING_ON_PRU1" ]; then
        ENTRY="AM62x_PRU1.cmd"
    fi
done

/usr/bin/cpp -P -I/opt/fpp/src/pru $@ $CPPFLAG /opt/fpp/src/pru/PRUSerial.asm > /tmp/PRUSerial.asm
clpru -v3 -o $@ $CPPFLAG --endian=little --hardware_mac=on /tmp/PRUSerial.asm
clpru -v3 $CPPFLAG -z /opt/fpp/src/pru/$ENTRY -o PRUSerial.out PRUSerial.obj -i/usr/share/ti/cgt-pru/lib -i/usr/share/ti/cgt-pru/include --library=libc.a
