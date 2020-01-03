#!/bin/bash
#####################################
# Need to install boost date-time to support the even/odd scheduler and other updates

apt-get -y update
apt-get -y --reinstall install libboost-filesystem-dev libboost-system-dev libboost-iostreams-dev libboost-date-time-dev  libboost-atomic-dev libboost-math-dev libboost-signals-dev

# Cleanup the apt stuff to free up space
apt-get clean
apt autoremove

