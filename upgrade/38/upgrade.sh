#!/bin/bash
#####################################
# This script enables CGI on the Apache server and sets up Apache to be able
# to serve the local /opt/fpp/.git directory as a repository.  This will allow
# remotes to pull in updates from a Master rather than going out to the
# internet while also ensuring that Remotes are running the same code as
# the Master system.

# Enable Apache2's CGI module
echo "Enabling Apache CGI module"
a2enmod cgi

# Copy new Apache site config into place
echo "Copying new Apache config into place"
sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" < ${FPPDIR}/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Restart Apache
#echo "Restarting Apache...  This will cause you to lose connectivity if upgrading via web browser."
#nohup systemctl restart apache2 > /dev/null 2>&1 &

