#!/bin/sh

echo $0 $@
cd /tmp

PRU=$1
shift

# clpru versions
/usr/bin/cpp -P -I/opt/fpp/src/pru $@ "/opt/fpp/src/non-gpl/BBShiftString/BBShiftString.asm" > "/tmp/BBShiftString_pru${PRU}.asm"
clpru -v3 -o  $@ "/tmp/BBShiftString_pru${PRU}.asm"
clpru -v3 -z --entry_point main /opt/fpp/src/pru/AM335x_PRU.cmd -o "BBShiftString_pru${PRU}.out" "BBShiftString_pru${PRU}.obj"
#objdump -h BBShiftString.out | grep .text |  awk '{print "dd if='BBShiftString.out' of='BBShiftString.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

