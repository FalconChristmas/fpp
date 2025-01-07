#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

#enable mod expires
a2enmod expires
#copy across new apache conf
cat /opt/fpp/etc/apache2.site >/etc/apache2/sites-enabled/000-default.conf
#restart apache
sudo service apache2 restart