#!/bin/bash
#####################################
# Upgrade 143: Serve plugin assets that live in a subdirectory again
#
# The plugin asset rewrite split the request path with a greedy
#
#     RewriteRule ^(.*)/(.*)$ /plugin.php?plugin=$1&file=$2&nopage=1
#
# so /images/plugin/<plugin>/images/foo.png arrived at plugin.php as
# plugin="<plugin>/images", file="foo.png". That used to work because plugin.php
# simply concatenated the two halves back together. It no longer does: the
# traversal hardening added a strict [A-Za-z0-9_.-] allow-list on the plugin
# name, which the embedded slash fails, so the name is blanked and every asset
# one directory deep 404s. Flat assets (icon.png) kept working, which is why
# this looked like an Apache problem rather than a plugin.php one.
#
# The same-commit fix splits on the FIRST slash instead -- ^([^/]+)/(.+)$ -- so
# the plugin name is one path component and the rest is the file, which
# plugin.php already resolves through subdirectories (and still refuses to
# leave the plugin directory).

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# Copy across the new apache conf with the corrected plugin asset rewrites.
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Gracefully reload apache config
gracefullyReloadApacheConf
