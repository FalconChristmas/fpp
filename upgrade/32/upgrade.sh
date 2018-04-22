#!/bin/bash
#
# Install libav libraries needed for Video processing in SDLOutput extension
#

apt-get -y update
apt-get -y install libswscale-dev libavdevice-dev libavfilter-dev

