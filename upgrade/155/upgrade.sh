#!/bin/bash
#####################################
# Upgrade 155: refuse proxied requests as "local"
#
# The UI password's "Require local" also let in requests that /proxy/ sent
# back to this player's own address: anyone on the LAN skipped the password
# that way.  Install the Apache site that marks proxied requests
# (X-FPP-Proxied) and refuses them as local, and add the same test to an
# existing password config, which is only rewritten when the password
# setting changes.
#
# Idempotent - safe to re-run.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 155: refuse proxied requests as local"

cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# A password config with "Require local" is rewritten whole, in the form
# SetupHtaccess(1) in www/common/settings.php now writes, rather than edited
# in place: an older layout without <RequireAll> would have taken an added
# "Require expr" line as one more alternative, letting every request in.
PWCONF="${MEDIADIR}/config/ui-password-config.conf"
if [ -f "${PWCONF}" ] && grep -q '^[[:space:]]*Require local' "${PWCONF}" && ! grep -q 'X-FPP-Proxied' "${PWCONF}"; then
    cat > "${PWCONF}" <<EOF
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
EOF
fi

gracefullyReloadApacheConf
