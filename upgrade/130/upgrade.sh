#!/bin/bash
#####################################
# Upgrade 130: Make the Apache port 80 listener survive a box with no IPv6
#
# configure_apache() has been pointing ports.conf at the IPv6 wildcard
# ("Listen [::]:80") so that one dual-stack socket serves both families.
# That is the right listener when the kernel has IPv6 -- and a hard failure
# when it does not:
#
#   AH00078: alloc_listener: failed to get a socket for ::
#   AH00526: Syntax error on line 5 of /etc/apache2/ports.conf
#   Listen setup failed
#
# Apache then refuses to start at all, so the entire web UI is gone and the
# player is only reachable over ssh. Any box booted with ipv6.disable=1
# lands here, as does any box running a kernel whose ipv6 module is missing.
#
# Replace it with a listener chosen by the config itself: <IfFile> is
# evaluated on every config parse -- start, configtest, and the SIGUSR1
# re-read behind "systemctl reload" -- so the probe re-runs continuously and a
# box that gains or loses IPv6 later corrects itself with no reinstall.
#
# Existing installs already have the unconditional ports.conf on disk, so
# this has to be rewritten here rather than waiting for the next OS image.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 130: Make the Apache port 80 listener IPv6-optional"

if [[ -f /etc/debian_version ]] && [[ -f /etc/apache2/ports.conf ]]; then
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

    # An earlier FPP appended an APACHE_ARGUMENTS define here to do this job
    # from envvars instead. ports.conf probes for itself now, so drop it.
    if grep -q FPP_HAVE_IPV6 /etc/apache2/envvars 2>/dev/null; then
        sed -i '/^## FPP: only ask apache for the IPv6/,/^fi$/d' /etc/apache2/envvars
    fi

    # Deliberately no restart and no reload of a *running* apache.
    #
    # Both branches above are evaluated at parse time, so a running master is
    # already listening on whichever one this file now selects -- the new
    # config is a no-op for it, and only matters at its next start. Restarting
    # to "apply" it would drop every connection, including the one streaming
    # this upgrade's output to the browser (manualUpdate.php runs behind this
    # very apache), which leaves the user watching a dead upgrade dialog with
    # no idea whether the rest of the run succeeded.
    #
    # The stopped case is the one worth acting on: on a box with no IPv6
    # apache is already down from the bug this fixes, nobody is watching a web
    # UI, and starting it here is the recovery.
    if systemctl is-active --quiet apache2; then
        echo "  Apache is running; the new listener config applies at its next start."
    else
        echo "  Apache is not running -- starting it with the new listener configuration"
        systemctl start apache2 || true
    fi
fi

exit 0
