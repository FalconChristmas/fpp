#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

#regenerate apache csp file to bring in change required for file manager  (and Gracefully reload apache config)
sudo /opt/fpp/scripts/ManageApacheContentPolicy.sh regenerate