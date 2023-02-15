#!/bin/bash
#####################################
## Add new rsync module into the rsync config that represents the mount location where USB device is mounted so that a remote host can put files here

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "
[remote_filecopy]
path = /mnt/remote_filecopy
read only = no
list = no
uid = fpp
gid = fpp
hosts allow = 127.0.0.0/8 172.16.0.0/12 192.168.0.0/16 10.0.0.0/8

" >> /etc/rsyncd.conf

rsyncEnabled=$(getSetting Service_rsync)

if [ "$rsyncEnabled" == 1 ]; then
   service rsync restart
fi

