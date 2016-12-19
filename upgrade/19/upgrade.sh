#!/bin/bash
#
# Configure shellinabox to use /var/tmp
#

sudo bash -c "echo 'SHELLINABOX_DATADIR=/var/tmp/' >> /etc/default/shellinabox"
