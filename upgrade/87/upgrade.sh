#!/bin/bash
#####################################


BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

git config --global --unset-all safe.directory "/opt/fpp"
git config --global --add safe.directory /opt/fpp