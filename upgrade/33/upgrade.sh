#!/bin/bash


# Need a new fppinit.service file that depends on var/tmp mount, root fs check, etc... to make sure 
# the filesystem is in a state where fppinit can actually work
cp /opt/fpp/etc/systemd/fppinit.service /lib/systemd/system

# prefer IPV4 for now as very little of what we do can even use IPV6
echo "precedence ::ffff:0:0/96  100" >>  /etc/gai.conf

# make sure we use the performance governor, required on BBB, helps on Pi
echo "GOVERNOR=performance" > /etc/default/cpufrequtils
apt-get install cpufrequtils

# Mark to reboot
sed -i -e "s/^rebootFlag .*/rebootFlag = 1/" /home/fpp/media/settings
