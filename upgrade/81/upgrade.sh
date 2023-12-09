#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

#Purge any old backups, leaving 45 of the most recent files
#This is a initial bypass of the backup script cleanup which may run out of memory if there are thousand of backups to delete
if [ -d ${MEDIADIR}/config/backups ]; then
  cd ${MEDIADIR}/config/backups
  ls -tr | head -n -45 | xargs --no-run-if-empty rm
fi