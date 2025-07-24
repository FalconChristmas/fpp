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

copy_desc=""
while IFS= read -r line; do
    # Bring across only description comments and IP-specific rules
    if [[ "$line" =~ ^#\ D: ]]; then
        # Save description to add before next IP rule
        copy_desc="$line"
    elif [[ "$line" =~ ^RewriteRule\ \^([0-9]{1,3}\.){3}[0-9]{1,3} ]]; then
        # If there's a saved description, write it before the rule
        if [ -n "$copy_desc" ]; then
            echo "$copy_desc" >> "$NEW_CONFIG"
            copy_desc=""
        fi
        echo "$line" >> "$NEW_CONFIG"
    fi
done < "$OLD_PROXIES"

# Set permissions
sudo chown fpp:fpp "$NEW_CONFIG"
sudo chmod 755 "$NEW_CONFIG"

# Gracefully reload apache config
gracefullyReloadApacheConf
