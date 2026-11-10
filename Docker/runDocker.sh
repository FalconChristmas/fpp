#!/bin/bash
if [ ! -f /opt/fpp/src/fppinit ]; then
    cd /opt/fpp/src
    CPUS=$(. /opt/fpp/scripts/common; . /opt/fpp/scripts/functions; ComputeMakeParallelism)
    make -j ${CPUS}
fi

# Detect if running in a container and flag accordingly
if [ -f "/.dockerenv" ]
then
	echo "docker" > /etc/fpp/container
elif [ -f "/run/.containerenv" -o -f "/var/run/.containerenv" ]
then
	echo "podman" > /etc/fpp/container
elif [ -f "/etc/fpp/container" ]
then
	rm /etc/fpp/container
fi

# No systemd in the container, so start the system bus and Avahi here.
# fppd's mDNS backend talks to avahi-daemon over D-Bus (warning 43 without it).
if [ -x /usr/bin/dbus-daemon ] && [ -x /usr/sbin/avahi-daemon ]; then
    mkdir -p /run/dbus
    rm -f /run/dbus/pid
    dbus-daemon --system --fork
    avahi-daemon --no-rlimits -D
fi

/opt/fpp/src/fppinit start
/opt/fpp/scripts/fppd_start

mkdir /run/php
/usr/sbin/php-fpm8.4 --fpm-config /etc/php/8.4/fpm/php-fpm.conf
/usr/sbin/apache2ctl -D FOREGROUND
