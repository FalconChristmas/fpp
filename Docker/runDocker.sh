#!/bin/bash
/usr/sbin/apache2ctl start
cd /opt/fpp/src
make
./fppd -f
