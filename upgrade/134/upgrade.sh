#!/bin/bash
#####################################
# Upgrade 134: Let the browser revalidate js/css after an FPP upgrade
#
# The cache-control block set every asset to
#
#     ExpiresDefault "access plus 1 year"
#     Header merge Cache-Control immutable
#
# with js and css excluded from only the first half of that pair -- they were
# not in the FilesMatch that strips `immutable` back off. So js/css went out as
#
#     Cache-Control: max-age=31536000, immutable
#
# `immutable` is the dangerous half. max-age alone still lets a normal reload
# revalidate; `immutable` is an explicit promise that the bytes at this URL will
# never change, and browsers honour it by not issuing a conditional GET at all.
# The only escape is a hard reload (Ctrl+F5), which nobody thinks to try
# because nothing about the failure looks cache-shaped.
#
# That promise is only true for assets whose URL changes when the file does.
# FPP appends ?ref=<filemtime> for exactly that reason -- but the tags were not
# all doing it. filemanager.php loaded bootstrap-table.min.js and its two
# extensions bare, while the FPP script that drives them,
# fpp-bootstrap-table.js, did carry a buster. Upgrade FPP and you get today's
# caller against a cached year-old library: InitializeBootstrapTable() throws,
# and because it is on the init path for every tab, the whole file manager
# renders empty tables while api/files/* keeps answering perfectly. Reported as
# "the file manager stopped showing files but the API is fine".
#
# The same-commit fix adds ?ref= to those tags and to the rest of the unbusted
# vendor js/css in filemanager.php and common/menuHead.inc. This upgrade is the
# other half: js and css now also get `immutable` stripped, so the next tag
# somebody adds without a buster degrades to a stale-until-reloaded asset
# instead of a permanently stuck one. Cost is one conditional GET per asset per
# session -- a 304 with no body -- against assets that still carry a one-year
# max-age, so nothing re-downloads while the mtime is unchanged.

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# Copy across the new apache conf, which adds css and m?js to the FilesMatch
# that removes `immutable` from Cache-Control.
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Gracefully reload apache config
gracefullyReloadApacheConf
