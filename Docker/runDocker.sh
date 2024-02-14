#!/bin/bash
if [ ! -f /opt/fpp/src/fpp ]; then
    cd /opt/fpp/src
    make -j 4
fi
/opt/fpp/src/fppinit start
/opt/fpp/scripts/fppd_start

mkdir /run/php
/usr/sbin/php-fpm8.2 --fpm-config /etc/php/8.2/fpm/php-fpm.conf
/usr/sbin/apache2ctl -D FOREGROUND
