#!/bin/bash
#
# update for master script
#
# Latest changes in master require libboost-dev and libjsoncpp-dev
#

sudo apt-get -q -y update
sudo apt-get -q -y --reinstall install libboost-dev libjsoncpp-dev
