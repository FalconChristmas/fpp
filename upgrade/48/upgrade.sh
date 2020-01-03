#!/bin/bash
#####################################
# This script enables mod-header on Apache 2 which is needed to set the CORS headers
# for stuff in the API directory

# Enable Apache modules
echo "Enabling Apache Header modules"
a2enmod headers


BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common
setSetting rebootFlag 1
echo "A reboot will be required to get the new /api directory working"

