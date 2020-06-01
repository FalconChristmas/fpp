#!/bin/sh

cd /tmp

echo $0 $@
/bin/rm -f /opt/fpp/lib/FalconMatrix.*
/bin/rm -f /opt/fpp/lib/FalconMatrixPRUCpy.*


# pasm versions...
/usr/bin/cpp -P $@ /opt/fpp/src/pru/FalconMatrix.p > /tmp/FalconMatrix.i
/usr/local/bin/pasm -V3 -b /tmp/FalconMatrix.i /tmp/FalconMatrix

/usr/bin/cpp -P $@ /opt/fpp/src/pru/FalconMatrixPRUCpy.p > /tmp/FalconMatrixPRUCpy.i
/usr/local/bin/pasm -V3 -b /tmp/FalconMatrixPRUCpy.i /tmp/FalconMatrixPRUCpy


# clpru versions
# For UIO, we need to pull the .text segment out of the ELF file which is what the objdump does
# When we go remoteproc, that won't be needed and the .out will be copied to the appropriate firmware
# location in /lib/firmware
#/usr/bin/cpp -P $@ /opt/fpp/src/pru/FalconMatrix.asm > /tmp/FalconMatrix.asm
#clpru -v3 -o $@ /tmp/FalconMatrix.asm
#clpru -v3 -z --entry_point main -o FalconMatrix.out FalconMatrix.obj
#objdump -h FalconMatrix.out | grep .text |  awk '{print "dd if='FalconMatrix.out' of='FalconMatrix.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash


#clpru -v3 -o $@ /opt/fpp/src/pru/FalconMatrixPRUCpy.asm
#clpru -v3 -z --entry_point main -o FalconMatrixPRUCpy.out FalconMatrixPRUCpy.obj
#objdump -h FalconMatrixPRUCpy.out | grep .text |  awk '{print "dd if='FalconMatrixPRUCpy.out' of='FalconMatrixPRUCpy.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash


