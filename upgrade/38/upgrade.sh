#!/bin/bash
#####################################
# This script enables CGI on the Apache server and sets up Apache to be able
# to serve the local /opt/fpp/.git directory as a repository.  This will allow
# remotes to pull in updates from a Master rather than going out to the
# internet while also ensuring that Remotes are running the same code as
# the Master system.

# Enable Apache2's CGI module
a2enmod cgi

# Copy new Apache site config into place
sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" < ${FPPDIR}/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Restart Apache
nohup systemctl restart apache2 &

