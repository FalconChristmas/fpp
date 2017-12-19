#!/bin/sh

echo $@
/bin/rm -f /opt/fpp/lib/FalconMatrix.bin
/usr/bin/cpp $@ /opt/fpp/src/pru/FalconMatrix.p | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/FalconMatrix.i
/usr/local/bin/pasm -V3 -b /tmp/FalconMatrix.i /tmp/FalconMatrix
#    /bin/rm /tmp/FalconMatrix.i
