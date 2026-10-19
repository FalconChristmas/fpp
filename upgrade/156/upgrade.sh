#!/bin/bash
#####################################
# Upgrade 156: repair a UI password config the first upgrade 155 broke; push
# the apache site config again
#
# The first version of upgrade 155 added
#     Require expr "%{HTTP:X-FPP-Proxied} == ''"
# on the line after "Require local" in an existing password config.  In a
# config of the older shape, whose Require lines sit side by side in one
# <RequireAny>, that line is one more alternative: every request not sent
# through /proxy/ got in without the password.  Upgrade 155 was rewritten
# since, but a player that ran the first version is past 155 for good.
#
# A password config (one with "Require valid-user") is rewritten whole, to
# what SetupHtaccess(1) in www/common/settings.php writes, whatever shape it
# is in now; FPP regenerates this file whenever the password setting changes,
# so nothing of the user's is lost.  An open (no password) config is left
# alone.
#
# The site config changed after a player could have run 155 (the sandbox on
# virtualdisplay_assets, the loopback kiosk's /proxy exemption, the gate for
# an older player's /proxy), and git updates never refresh it, so it is
# pushed again.
#
# Idempotent - safe to re-run.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 156: repair the UI password config, update the apache site config"

PWCONF="${MEDIADIR}/config/ui-password-config.conf"
if [ -f "${PWCONF}" ] && grep -q 'Require[[:space:]]\+valid-user' "${PWCONF}"; then
    cat > "${PWCONF}" <<EOF2
AuthUserFile ${MEDIADIR}/config/.htpasswd
AuthType Basic
AuthName "Falcon Player"
<RequireAny>
  <RequireAll>
    Require local
    Require expr "%{HTTP:X-FPP-Proxied} == ''"
  </RequireAll>
  <RequireAll>
    Require valid-user
  </RequireAll>
</RequireAny>

SetEnvIf Host ^ LOCAL_PROTECT=1

<FilesMatch "^(favicon)\.ico\$">
Allow from All
Satisfy Any
</FilesMatch>
EOF2
    echo "  Rewrote ${PWCONF}"
fi

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    # FPP_Install_Mac.sh generates its httpd conf from etc/apache2.site with the
    # paths rewritten for the Homebrew prefix, so a straight copy would be wrong.
    echo "  Not pushing the site config on MacOS - re-run SD/FPP_Install_Mac.sh to pick it up"
else
    cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf
fi

# A graceful reload lets the requests in flight, this upgrade's own output
# among them, finish.  No Apache running (an image build) is not a failure.
gracefullyReloadApacheConf || true
exit 0
