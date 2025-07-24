#!/bin/bash
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

#copy across new apache conf to version which includes previous .htaccess based rewrites
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

#regenerate apache csp file to bring in change required for connect-src
sudo /opt/fpp/scripts/ManageApacheContentPolicy.sh regenerate


##### MIGRATE old to new format proxy config #####
OLD_PROXIES="/home/fpp/media/config/proxies"
NEW_CONFIG="/home/fpp/media/config/proxy-config.conf"

# Only migrate if old proxies file exists
if [ ! -f "$OLD_PROXIES" ]; then
    echo "No old proxies file found, nothing to migrate."
    exit 0
fi

# Backup existing new config if present
if [ -f "$NEW_CONFIG" ]; then
    cp "$NEW_CONFIG" "$NEW_CONFIG.bak.$(date +%s)"
fi

# Prepare new config file
> "$NEW_CONFIG"

while read -r line; do
    # Skip empty lines and comments
    [[ -z "$line" || "$line" =~ ^# ]] && continue

    # Extract host and optional description (assume format: host [# D:description])
    host=$(echo "$line" | awk '{print $1}')
    description=$(echo "$line" | grep -o '# D:.*' | sed 's/# D://')

    # Write description if present
    if [ -n "$description" ]; then
        echo "# D:$description" >> "$NEW_CONFIG"
    fi

    # Write rewrite rules
    echo "RewriteRule ^${host}$  ${host}/  [R,L]" >> "$NEW_CONFIG"
    echo "RewriteRule ^${host}/(.*)$  http://${host}/\$1  [P,L]" >> "$NEW_CONFIG"
    echo "" >> "$NEW_CONFIG"
done < "$OLD_PROXIES"

# Set permissions
sudo chown fpp:fpp "$NEW_CONFIG"
sudo chmod 755 "$NEW_CONFIG"


# Gracefully reload apache config
gracefullyReloadApacheConf
