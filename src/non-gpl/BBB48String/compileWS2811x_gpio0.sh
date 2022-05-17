#!/bin/sh

echo $0 $@
cd /tmp

# clpru versions
/usr/bin/cpp -P -I/opt/fpp/src/pru $@ /opt/fpp/src/non-gpl/BBB48String/FalconWS281x.asm > /tmp/FalconWS281x_gpio0.asm
clpru -v3 -o $@ /tmp/FalconWS281x_gpio0.asm
clpru -v3 -z --entry_point main /opt/fpp/src/pru/AM335x_PRU.cmd  -o FalconWS281x_gpio0.out FalconWS281x_gpio0.obj
#objdump -h FalconWS281x_gpio0.out | grep .text |  awk '{print "dd if='FalconWS281x_gpio0.out' of='FalconWS281x_gpio0.bin' bs=1 count=$[0x" $3 "] skip=$[0x" $6 "]"}'  | bash

