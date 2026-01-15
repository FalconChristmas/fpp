#!/bin/bash
if [ ! -f /opt/fpp/src/fppinit ]; then
    cd /opt/fpp/src
    CPUS=$(/opt/fpp/scripts/functions ComputeMakeParallelism)
    make -j ${CPUS}
fi

/opt/fpp/src/fppinit start
/opt/fpp/scripts/fppd_start

mkdir /run/php
/usr/sbin/php-fpm8.4 --fpm-config /etc/php/8.4/fpm/php-fpm.conf
/usr/sbin/apache2ctl -D FOREGROUND
