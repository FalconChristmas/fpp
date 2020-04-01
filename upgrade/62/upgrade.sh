#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "Configuring ccache to use /tmp, 250M, setup for using precompiled headers"
ccache -M 250M
ccache --set-config=temporary_dir=/tmp
ccache --set-config=sloppiness=pch_defines,time_macros

