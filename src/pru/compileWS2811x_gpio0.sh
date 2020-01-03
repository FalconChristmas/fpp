#!/bin/sh

echo $0 $@
/bin/rm -f /opt/fpp/lib/FalconWS281x_gpio0.bin
/usr/bin/cpp $@ /opt/fpp/src/pru/FalconWS281x.p | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/FalconPRU_gpio0.i
/usr/local/bin/pasm -V3 -b /tmp/FalconPRU_gpio0.i /tmp/FalconWS281x_gpio0
#    /bin/rm /tmp/FalconPRU_gpio0.i
