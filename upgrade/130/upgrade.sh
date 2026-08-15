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
# Replace it with a listener chosen at apache *start* time rather than at
# install time: apachectl sources /etc/apache2/envvars on every
# start/restart/configtest, so the probe re-runs on each start and a box
# that gains or loses IPv6 later corrects itself with no reinstall.
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
# FPP_HAVE_IPV6 is defined from /etc/apache2/envvars when the running
# kernel actually has IPv6, so a box without it still serves over IPv4
# instead of failing to start apache at all.
<IfDefine FPP_HAVE_IPV6>
Listen [::]:80
</IfDefine>
<IfDefine !FPP_HAVE_IPV6>
Listen 80
</IfDefine>

<IfModule ssl_module>
	Listen 443
</IfModule>

<IfModule mod_gnutls.c>
	Listen 443
</IfModule>
PORTS_EOF

    if ! grep -q FPP_HAVE_IPV6 /etc/apache2/envvars; then
        cat >> /etc/apache2/envvars <<'ENVVARS_EOF'

## FPP: only ask apache for the IPv6 wildcard listener when the running
## kernel has IPv6. /proc/sys/net/ipv6 is absent both when the module is
## missing and when the kernel booted with ipv6.disable=1, which are exactly
## the cases where "Listen [::]:80" aborts apache startup.
if [ -d /proc/sys/net/ipv6 ]; then
	export APACHE_ARGUMENTS="${APACHE_ARGUMENTS} -D FPP_HAVE_IPV6"
fi
ENVVARS_EOF
    fi

    # Restart, NOT gracefullyReloadApacheConf. Two reasons this specific
    # change cannot go out over a graceful reload, both verified on a Pi5:
    #
    #  1. FPP_HAVE_IPV6 comes from the master process's command line, which
    #     apachectl builds from envvars at *start*. A running master re-reads
    #     ports.conf on SIGUSR1 using the defines it was originally started
    #     with -- so a master started before this upgrade has no
    #     FPP_HAVE_IPV6 and takes the "Listen 80" branch.
    #  2. A graceful restart cannot change listening sockets at all.
    #
    # Together those kill the server outright: reloading a pre-upgrade master
    # against the new ports.conf leaves apache 'failed' with no listener on
    # port 80 and the web UI gone until the next reboot.
    #
    # The stopped case matters too: on a box with no IPv6, apache is already
    # down from the bug this fixes, and starting it here is the recovery.
    echo "  Restarting Apache to pick up the new listener configuration"
    systemctl restart apache2 || true
fi

exit 0
