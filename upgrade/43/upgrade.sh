#!/bin/bash
#
# Install GraphicsMagick Magick++ for PlaylistEntryImage
# Reinstall boost libraries to fix compile issue
#

apt-get -y update
apt-get -y install libgraphicsmagick++1-dev graphicsmagick-libmagick-dev-compat
apt-get -y --reinstall install libboost-filesystem-dev libboost-system-dev libboost-iostreams-dev
