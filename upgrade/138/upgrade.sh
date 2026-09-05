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

# Set the reboot flag so the new vhost is picked up.  Reloading apache or php-fpm
# from here would cut the connection this upgrade's own output is streaming over
# and hang the update.
setSetting rebootFlag 1
