#!/bin/sh

echo $0 $@
cd /tmp
/bin/rm -f /opt/fpp/lib/FalconMatrix.*
/bin/rm -f /opt/fpp/lib/FalconMatrixPRUCpy.*

# clpru versions
/usr/bin/cpp -P $@ /opt/fpp/src/pru/FalconMatrix.asm > /tmp/FalconMatrix.asm
clpru -v3 -o $@ /tmp/FalconMatrix.asm
clpru -v3 -z --entry_point main /opt/fpp/src/pru/AM335x_PRU.cmd -o FalconMatrix.out FalconMatrix.obj
#objdump -h FalconMatrix.out | grep .text |  awk '{print "dd if='FalconMatrix.out' of='FalconMatrix.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash


clpru -v3 -o $@ /opt/fpp/src/pru/FalconMatrixPRUCpy.asm
clpru -v3 -z --entry_point main /opt/fpp/src/pru/AM335x_PRU.cmd -o FalconMatrixPRUCpy.out FalconMatrixPRUCpy.obj
#objdump -h FalconMatrixPRUCpy.out | grep .text |  awk '{print "dd if='FalconMatrixPRUCpy.out' of='FalconMatrixPRUCpy.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash


