#!/bin/sh

echo $0 $@
/bin/rm -f /opt/fpp/lib/FalconWS281x.bin
/usr/bin/cpp -P $@ /opt/fpp/src/pru/FalconWS281x.p  > /tmp/FalconPRU.i
/usr/local/bin/pasm -V3 -b /tmp/FalconPRU.i /tmp/FalconWS281x


# clpru versions
# For UIO, we need to pull the .text segment out of the ELF file which is what the objdump does
# When we go remoteproc, that won't be needed and the .out will be copied to the appropriate firmware
# location in /lib/firmware
#cd /tmp
#/usr/bin/cpp -P $@ /opt/fpp/src/pru/FalconWS281x.asm > /tmp/FalconWS281x.asm
#clpru -v3 -o $@ /tmp/FalconWS281x.asm
#clpru -v3 -z --entry_point main -o FalconWS281x.out FalconWS281x.obj
#objdump -h FalconWS281x.out | grep .text |  awk '{print "dd if='FalconWS281x.out' of='FalconWS281x.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

