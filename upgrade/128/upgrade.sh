#!/bin/bash
#####################################
# Upgrade 128: Regenerate the CSP header for the fonts.scalar.com font-src fix
#
# scripts/ManageApacheContentPolicy.sh's DEFAULT_VALUES["font-src"] gained
# https://fonts.scalar.com (the API docs page's webfonts host -- missing
# since Scalar was integrated, confirmed live via 14 blocked font-src CSP
# violations on /api/, silently falling back to the system font instead of
# erroring visibly).
#
# etc/apache2.csp is a GENERATED file, rebuilt from that script's
# DEFAULT_VALUES plus the user's local override JSON every time
# ManageApacheContentPolicy.sh runs -- normally only at boot, via FPPINIT's
# handle_boot_actions. A plain git-pull upgrade rebuilds and restarts fppd
# but never reboots the device and never calls this script, so without this
# step the fix would sit inert on an already-running box until its next
# physical reboot, which for a show controller may not happen for a long
# time. Force the regeneration now, same reasoning as upgrade/123 forcing
# an apache2.site recopy rather than waiting for one.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 128: Regenerate CSP header for fonts.scalar.com fix"

${FPPDIR}/scripts/ManageApacheContentPolicy.sh regenerate

exit 0
