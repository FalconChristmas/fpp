#!/bin/bash
# Fix broken and missing email system
#

apt-get update
#this will install exim4 also
apt-get -q -y install mailutils

#Get FPP hostname
FPP_HOSTNAME=$(cat /etc/hostname 2> /dev/null)

# Update exim4 config for gmail
cat <<-EOF > /etc/exim4/update-exim4.conf.conf
# /etc/exim4/update-exim4.conf.conf
#
# Edit this file and /etc/mailname by hand and execute update-exim4.conf
# yourself or use 'dpkg-reconfigure exim4-config'
#
# Please note that this is _not_ a dpkg-conffile and that automatic changes
# to this file might happen. The code handling this will honor your local
# changes, so this is usually fine, but will break local schemes that mess
# around with multiple versions of the file.
#
# update-exim4.conf uses this file to determine variable values to generate
# exim configuration macros for the configuration file.
#
# Most settings found in here do have corresponding questions in the
# Debconf configuration, but not all of them.
#
# This is a Debian specific file
dc_eximconfig_configtype='smarthost'
dc_other_hostnames='fpp; ${FPP_HOSTNAME}'
dc_local_interfaces='127.0.0.1'
dc_readhost=''
dc_relay_domains=''
dc_minimaldns='false'
dc_relay_nets=''
dc_smarthost='smtp.gmail.com::587'
CFILEMODE='644'
dc_use_split_config='true'
dc_hide_mailname='false'
dc_mailname_in_oh='true'
dc_localdelivery='mail_spool'
EOF

#remove exim4 panic log so exim4 doesn't throw an alert about a non-zero log file
#due to some odd error thrown during inital setup
rm /var/log/exim4/paniclog

#update config and restart exim
update-exim4.conf
invoke-rc.d exim4 restart

