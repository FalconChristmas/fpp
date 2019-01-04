#!/bin/bash
#####################################
# This script enables CGI on the Apache server and sets up Apache to be able
# to serve the local /opt/fpp/.git directory as a repository.  This will allow
# remotes to pull in updates from a Master rather than going out to the
# internet while also ensuring that Remotes are running the same code as
# the Master system.

# Enable Apache modules
echo "Enabling Apache Rewrite and Proxy modules"
a2enmod rewrite
a2enmod proxy
a2enmod proxy_http

echo "A reboot will be required to get the new /api directory working"

