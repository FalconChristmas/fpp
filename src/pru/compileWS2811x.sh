#!/bin/sh

echo $@
/bin/rm -f /opt/fpp/lib/FalconWS281x.bin
/usr/bin/cpp $@ /opt/fpp/src/pru/FalconWS281x.p | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/FalconPRU.i
/usr/local/bin/pasm -V3 -b /tmp/FalconPRU.i /tmp/FalconWS281x
#    /bin/rm /tmp/FalconPRU.i
