#!/bin/sh

echo $0 $@
cd /tmp
/bin/rm -f /opt/fpp/lib/FalconSerial.*

# clpru versions
/usr/bin/cpp -P $@ /opt/fpp/src/pru/FalconSerial.asm > /tmp/FalconSerial.asm
clpru -v3 -o $@ /tmp/FalconSerial.asm
clpru -v3 -z --entry_point main  /opt/fpp/src/pru/AM335x_PRU.cmd  -o FalconSerial.out FalconSerial.obj
#objdump -h FalconSerial.out | grep .text |  awk '{print "dd if='FalconSerial.out' of='FalconSerial.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

