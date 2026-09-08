#!/bin/bash
#####################################
# Upgrade 139: Regenerate the CSP header, dropping fonts.scalar.com
#
# This reverses upgrade/128.  That one ADDED https://fonts.scalar.com to
# scripts/ManageApacheContentPolicy.sh's DEFAULT_VALUES["font-src"] so the
# Scalar API docs viewer could load its webfonts.  Allowing the fetch was the
# wrong fix -- it made every visit to /api/ send the visitor's IP to a host the
# operator never chose, and on the isolated networks these controllers usually
# run on the fetch just stalls until it times out.  www/api/api.html now passes
# "withDefaultFonts": false, so Scalar never injects those @font-face rules and
# no external font is requested at all -- the allowance is dead weight, and
# leaving it in the header would keep permitting a request we no longer make.
#
# etc/apache2.csp is a GENERATED file, rebuilt from that script's DEFAULT_VALUES
# plus the user's local override JSON every time ManageApacheContentPolicy.sh
# runs -- normally only at boot, via FPPINIT's handle_boot_actions.  A plain
# git-pull upgrade rebuilds and restarts fppd but never reboots the device and
# never calls this script, so without this step the old header would sit on an
# already-running box until its next physical reboot, which for a show
# controller may not happen for a long time.  Regenerate the file now and set
# the reboot flag so the new header actually takes effect.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 139: Regenerate CSP header (drop fonts.scalar.com)"

# regenerate-norestart, not regenerate: the latter calls
# gracefullyReloadApacheConf, and reloading apache from inside an upgrade cuts
# the connection this script's own output is streaming over, hanging the
# update.  Write the new header now and let the reboot pick it up, same as
# upgrade/138 does for the vhost.
${FPPDIR}/scripts/ManageApacheContentPolicy.sh regenerate-norestart

setSetting rebootFlag 1

exit 0
