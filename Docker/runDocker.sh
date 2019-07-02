#!/bin/bash
/opt/fpp/scripts/init_pre_network
/opt/fpp/scripts/fppd_start

/usr/sbin/apache2ctl -D FOREGROUND
