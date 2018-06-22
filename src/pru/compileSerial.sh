#!/bin/sh

echo $@
/bin/rm -f /opt/fpp/lib/FalconSerial.bin
/usr/bin/cpp $@ /opt/fpp/src/pru/FalconSerial.p | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/FalconPRUSerial.i
/usr/local/bin/pasm -V3 -b /tmp/FalconPRUSerial.i /tmp/FalconSerial
#    /bin/rm /tmp/FalconPRU.i
