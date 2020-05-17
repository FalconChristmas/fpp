#!/bin/sh

echo $0 $@
/bin/rm -f /opt/fpp/lib/FalconWS281x_gpio0.bin
/usr/bin/cpp -P $@ /opt/fpp/src/pru/FalconWS281x.p > /tmp/FalconPRU_gpio0.i
/usr/local/bin/pasm -V3 -b /tmp/FalconPRU_gpio0.i /tmp/FalconWS281x_gpio0

cd /tmp
#/usr/bin/cpp -P $@ /opt/fpp/src/pru/FalconWS281x.asm > /tmp/FalconWS281x_gpio0.asm
#clpru -v3 -o $@ /tmp/FalconWS281x_gpio0.asm
#clpru -v3 -z --entry_point main -o FalconWS281x_gpio0.out FalconWS281x_gpio0.obj
#objdump -h FalconWS281x_gpio0.out | grep .text |  awk '{print "dd if='FalconWS281x_gpio0.out' of='FalconWS281x_gpio0.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

