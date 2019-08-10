#!/bin/sh

echo $0 $@
/bin/rm -f /opt/fpp/lib/FalconMatrix.bin
/bin/rm -f /opt/fpp/lib/FalconMatrixPRUCpy.bin
/usr/bin/cpp $@ /opt/fpp/src/pru/FalconMatrix.p | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/FalconMatrix.i
/usr/local/bin/pasm -V3 -b /tmp/FalconMatrix.i /tmp/FalconMatrix

/usr/bin/cpp $@ /opt/fpp/src/pru/FalconMatrixPRUCpy.p | perl -p -e 's/^#.*//; s/;/\n/g; s/BYTE\((\d+)\)/t\1/g' > /tmp/FalconMatrixPRUCpy.i
/usr/local/bin/pasm -V3 -b /tmp/FalconMatrixPRUCpy.i /tmp/FalconMatrixPRUCpy
#    /bin/rm /tmp/FalconMatrix.i
