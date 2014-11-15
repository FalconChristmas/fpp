#!/bin/bash
#
# security script
#
# Remove /etc/ssmtp/ssmtp.conf if stock
#

# Let's check against a known md5sum to ensure the file is the original one, not one somebody tweaked.
if [ "$(md5sum /etc/ssmtp/ssmtp.conf | awk '{print $1}')" == "2781d08375826839ea7530dee1dfeee6" ]; then
	rm -f /etc/ssmtp/ssmtp.conf
fi
