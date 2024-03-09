#!/bin/sh

echo $0 $@
cd /tmp

PRU=$1
shift

# clpru versions
/usr/bin/cpp -P -I/opt/fpp/src/pru $@ "/opt/fpp/src/non-gpl/FalconV5Support/FalconV5PRUListener.asm" > "/tmp/FalconV5Support_pru${PRU}.asm"
clpru -v3 -o  $@ "/tmp/FalconV5Support_pru${PRU}.asm"
clpru -v3 -z --entry_point main /opt/fpp/src/pru/AM335x_PRU.cmd -o "FalconV5Support_pru${PRU}.out" "FalconV5Support_pru${PRU}.obj"
#objdump -h FalconV5Support.out | grep .text |  awk '{print "dd if='FalconV5Support.out' of='FalconV5Support.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

