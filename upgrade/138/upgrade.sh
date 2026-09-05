#!/bin/bash
#####################################
# Upgrade 138: Push apache config that turns off FastCGI connection reuse
#
# etc/apache2.site had "enablereuse=on" on the php-fpm proxy worker.  With reuse
# on, each httpd thread keeps its own persistent connection to php-fpm and a
# connection is only ever served by the child that accepted it.  MaxRequestWorkers
# (75) is far larger than pm.max_children (25), so once 25 different httpd threads
# have each opened a connection, every child is pinned to one of them for good and
# none returns to the accept queue.  Requests arriving on any other thread then
# wait for a child that never comes, until ProxyTimeout -- 1200s in the same file
# -- so the whole UI stops loading until php-fpm is restarted.
#
# git_pull never syncs the vhost (only git_branch does), so existing installs keep
# the old file across updates and need this push.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 138: Update apache site config (php-fpm connection reuse)"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    # FPP_Install_Mac.sh generates its httpd conf from etc/apache2.site with the
    # paths rewritten for the Homebrew prefix, so a straight copy would be wrong.
    echo "  Skipping on MacOS - re-run SD/FPP_Install_Mac.sh to pick this up"
    exit 0
fi

# Follows the symlink on installs where sites-enabled points at sites-available.
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Gracefully reload apache config
gracefullyReloadApacheConf

# Children pinned by the old setting sit idle holding a connection, so a graceful
# apache reload leaves them stuck until ProxyTimeout.  Reloading php-fpm retires
# them now; reload rather than restart so the PHP request running this upgrade
# (an update started from the web UI) is allowed to finish.
FPMUNIT=$(systemctl list-units --type=service --state=active --no-legend 'php*-fpm.service' 2>/dev/null | awk '{print $1}' | head -1)
if [ -n "${FPMUNIT}" ]; then
    echo "  Reloading ${FPMUNIT} to release pinned workers"
    ${SUDO} systemctl reload ${FPMUNIT}
fi

exit 0
