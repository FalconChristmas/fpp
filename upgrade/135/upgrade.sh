#!/bin/bash
#####################################
# Upgrade 135: Let ports.conf decide the port 80 listener for itself
#
# Upgrade 130 made the IPv6 wildcard listener conditional so that a box
# without IPv6 still starts apache, but it did the probe in the wrong place:
#
#   /etc/apache2/envvars    if [ -d /proc/sys/net/ipv6 ]; then
#                               export APACHE_ARGUMENTS="... -D FPP_HAVE_IPV6"
#   /etc/apache2/ports.conf <IfDefine FPP_HAVE_IPV6>Listen [::]:80</IfDefine>
#
# apachectl sources envvars and puts -D FPP_HAVE_IPV6 on the master's command
# line, so the define exists only for a master *started* after 130 ran. A
# SIGUSR1 reload -- what "systemctl reload apache2" and FPP's own
# gracefullyReloadApacheConf do -- makes the running master re-parse ports.conf
# with the defines it was originally started with. A master started before 130
# therefore re-reads the new file, finds FPP_HAVE_IPV6 undefined, and takes the
# "Listen 80" branch on a box that is currently listening on [::]:80. Since a
# graceful restart cannot change listening sockets either, that combination
# leaves apache failed with nothing bound to port 80 -- the web UI gone until
# the next reboot -- and it is reachable from upgrade 133 and 134, both of
# which reload apache later in the same upgrade run.
#
# 130 worked around that by doing a full "systemctl restart apache2". That
# applies the change, and also drops the connection carrying the upgrade's own
# output to the browser (manualUpdate.php runs behind this apache), so the
# upgrade dialog dies mid-run.
#
# Move the probe into the config instead. <IfFile> is evaluated on every parse
# -- start, configtest, and the SIGUSR1 re-read -- so the file always selects
# the branch that matches the running kernel no matter how the master was
# started. A reload then agrees with what is already bound, there is nothing to
# restart, and a box that gains or loses IPv6 later still corrects itself.
#
# Needs apache 2.4.34+ for <IfFile>; the oldest release FPP installs on ships
# 2.4.38.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 135: Let ports.conf probe for IPv6 itself"

if [[ -f /etc/debian_version ]] && [[ -f /etc/apache2/ports.conf ]]; then
    # Only touch the file FPP wrote. A hand-edited or stock ports.conf has no
    # FPP_HAVE_IPV6 in it and is left alone.
    if grep -q FPP_HAVE_IPV6 /etc/apache2/ports.conf; then
        echo "  Rewriting /etc/apache2/ports.conf to probe /proc/sys/net/ipv6 directly"
        cat > /etc/apache2/ports.conf <<'PORTS_EOF'
# Managed by FPP -- see configure_apache() in SD/FPP_Install.sh.
# /proc/sys/net/ipv6 is absent both when the ipv6 module is missing and when
# the kernel booted with ipv6.disable=1 -- exactly the cases where
# "Listen [::]:80" aborts apache startup -- so such a box serves over IPv4
# instead of failing to start apache at all.
<IfFile /proc/sys/net/ipv6>
Listen [::]:80
</IfFile>
<IfFile !/proc/sys/net/ipv6>
Listen 80
</IfFile>

<IfModule ssl_module>
	Listen 443
</IfModule>

<IfModule mod_gnutls.c>
	Listen 443
</IfModule>
PORTS_EOF
    fi

    # Nothing reads FPP_HAVE_IPV6 any more, so take the block 130 appended back
    # out of envvars rather than leaving a define that looks load-bearing.
    if grep -q FPP_HAVE_IPV6 /etc/apache2/envvars 2>/dev/null; then
        echo "  Removing the now-unused FPP_HAVE_IPV6 define from /etc/apache2/envvars"
        sed -i '/^## FPP: only ask apache for the IPv6/,/^fi$/d' /etc/apache2/envvars
    fi

    # No restart, and no reload either. The running master is already bound to
    # whichever listener this file now selects, so the rewrite is a no-op for
    # it; restarting would only serve to kill the connection streaming this
    # upgrade to the browser. If apache is down -- the failure 130 set out to
    # fix -- starting it here is the recovery, and no web UI is watching.
    if systemctl is-active --quiet apache2; then
        echo "  Apache is running and already bound correctly; leaving it alone."
    else
        echo "  Apache is not running -- starting it with the new listener configuration"
        systemctl start apache2 || true
    fi
fi

exit 0
