#!/bin/bash
#
# Install zstd libraries needed for fseq 2.0 files
#

apt-get -y update
apt-get -y install zstd libzstd-dev
