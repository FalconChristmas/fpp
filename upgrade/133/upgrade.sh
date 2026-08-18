#!/bin/bash
#####################################
# Upgrade 133: Stop the /proxy handler re-compressing proxied responses
#
# The <Directory "/opt/fpp/www/proxy"> block carried
#
#     SetOutputFilter INFLATE;DEFLATE
#     ProxyHTMLBufSize 32768
#     ProxyHTMLStripComments Off
#     ProxyHTMLMeta On
#     ProxyHTMLDocType HTML
#     ProxyHTMLExtended On
#     ProxyHTMLCharsetOut UTF-8
#
# None of the ProxyHTML* directives were doing anything. They configure
# mod_proxy_html's output filter, and that filter is only inserted when
# something sets "ProxyHTMLEnable On" -- which neither FPP's config nor
# Debian's mods-available/proxy_html.conf does (it is commented out there).
#
# The INFLATE;DEFLATE chain exists to hand mod_proxy_html decompressed HTML to
# rewrite links in. With that filter never inserted, it was gunzipping and
# re-gzipping every byte we proxy -- js, css, fonts, images -- burning Pi CPU
# for a consumer that was not in the chain.
#
# It also changed the shape of the response. Apache cannot know the length of
# what it is about to compress, so it drops the controller's Content-Length and
# switches to Transfer-Encoding: chunked. That is what made
# FalconChristmas/fpp#2836 look the way it did. A Genius 2.x / Vivid board
# serves its Vite assets as
#
#     Content-Length: 17643
#     Content-Encoding: gzip
#
# and the very same request through /proxy/ came back re-framed as
# Transfer-Encoding: chunked with Content-Encoding stripped and a
# "Vary: Accept-Encoding" that mod_deflate had added. When the response then
# ended early the browser reported
#
#   GET http://fpp.local/proxy/192.168.68.60/assets/ext-DzpEBlUd.js
#       net::ERR_INCOMPLETE_CHUNKED_ENCODING 200 (OK)
#
# i.e. a 200 with a truncated body, so the SPA never finished preloading and sat
# at "Loading...". The chunking the report blamed FPP for was FPP's own, added
# by this filter -- the proxy relays a chunked upstream perfectly well.
#
# The filter did do one useful job, so it is kept for exactly that case.
# Controllers such as ESPixelStick store their assets pre-gzipped and have no
# CPU headroom to expand them, so they reply Content-Encoding: gzip even when
# the client never asked for it. A client that cannot decode gzip (curl and
# wget send no Accept-Encoding by default) would get a body it cannot read. So
# INFLATE now runs only for those clients:
#
#     <If "%{HTTP:Accept-Encoding} !~ /gzip/">
#         SetOutputFilter INFLATE
#     </If>
#
# Browsers all send Accept-Encoding: gzip, so they now get the controller's
# bytes through untouched, which is what fixes the Genius case. Note this is an
# output filter in both forms and never affected what we ask the controller FOR
# -- mod_proxy forwards the client's Accept-Encoding upstream verbatim either
# way.

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

# Copy across the new apache conf, which drops the unconditional
# INFLATE;DEFLATE output filter and the inert ProxyHTML* directives from the
# /proxy block.
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Gracefully reload apache config
gracefullyReloadApacheConf
