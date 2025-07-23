#!/bin/bash
##############################################################################
# Some internal setup
PATH="/sbin:/bin:/usr/sbin:/usr/bin"

BINDIR=$(cd $(dirname $0) && pwd)

. ${BINDIR}/common
. ${BINDIR}/functions

#############################################################################

# Path to your local CSP JSON override file
JSON_FILE=$MEDIADIR"/config/csp_allowed_domains.json"

# Default key values hard-coded to include in all configs
declare -A DEFAULT_VALUES
DEFAULT_VALUES=( 
    ["default-src"]="'self' http://www.w3.org"
    ["connect-src"]="'self' https://raw.githubusercontent.com https://ipapi.co https://kulplights.com https://www.hansonelectronics.com.au https://www.wiredwatts.com https://fppstats.falconchristmas.com"
    ["object-src"]="'none' "
    ["img-src"]="'self' blob: data: http://www.w3.org https://www.paypal.com https://www.paypalobjects.com"
    ["script-src"]="'self' 'unsafe-inline' 'unsafe-eval' https://api.falconplayer.com"
    ["style-src"]="'self' 'unsafe-inline'"
)

# local JSON template content
    json_struct_content='{
      "default-src": [],
      "img-src": [],
      "script-src": [],
      "style-src": [],
      "connect-src": [],
      "object-src": []
    }'

# Generate local CSP JSON override file if NOT exists
if [ ! -f $JSON_FILE ]; then
    # Write JSON template content to file
    echo "$json_struct_content" > $JSON_FILE
fi

# Function to check if a key exists in the JSON file
key_exists() {
    key=$1
    jq --arg key "$key" 'has($key)' $JSON_FILE
}

# Function to check if a domain exists under a specific key in the JSON file
domain_exists() {
    key=$1
    domain=$2
    jq --arg key "$key" --arg domain "$domain" '(.[$key] // []) | index($domain)' $JSON_FILE
}

# Function to add a domain to the JSON file
add_domain() {
    key=$1
    domain=$2
    if [ $(key_exists "$key") = true ] && [ $(domain_exists "$key" "$domain") = null ]; then
        jq --arg key "$key" --arg domain "$domain" '(.[$key] // []) |= . + [$domain]' $JSON_FILE > tmp.json && mv tmp.json $JSON_FILE
        echo "Domain '$domain' added under '$key'."
    else
        echo "Domain '$domain' already exists under '$key' or key doesn't exist."
    fi
}

# Function to remove a domain from the JSON file
remove_domain() {
    key=$1
    domain=$2
    if [ $(key_exists "$key") = true ] && [ $(domain_exists "$key" "$domain") != null ]; then
        jq --arg key "$key" --arg domain "$domain" '(.[$key] // []) |= map(select(. != $domain))' $JSON_FILE > tmp.json && mv tmp.json $JSON_FILE
        echo "Domain '$domain' removed from '$key'."
    else
        echo "Domain '$domain' doesn't exist under '$key' or key doesn't exist."
    fi
}

# Function to detect Cape Domains from cape config
detectCapeDomains() {
    if [ ! -f $MEDIADIR/tmp/cape-info.json ]; then
        return
    fi

    cape_json_data=$(cat $MEDIADIR/tmp/cape-info.json)

    # Extract base URLs
    header_cape_image_url=$(echo "$cape_json_data" | jq -r '.header_cape_image // empty' | sed 's|\(https\?://[^/]*\).*|\1|')
    vendor_image_url=$(echo "$cape_json_data" | jq -r '.vendor.image // empty' | sed 's|\(https\?://[^/]*\).*|\1|')
    vendor_url=$(echo "$cape_json_data" | jq -r '.vendor.url // empty' | sed 's|\(https\?://[^/]*\).*|\1|')
    vendor_landing_page_url=$(echo "$cape_json_data" | jq -r '.vendor.landingPage // empty' | sed 's|\(https\?://[^/]*\).*|\1|')

    # Combine URLs and remove duplicates
    urls=("$header_cape_image_url" "$vendor_image_url" "$vendor_url" "$vendor_landing_page_url")
    distinct_urls=($(printf "%s\n" "${urls[@]}" | sort -u))

    # Output the distinct URLs
    echo "${distinct_urls[@]}"
}

# Function to detect all IPv4 /24 subnets related to the interfaces the device is using
detect_systems_subnets() {
  ip_addresses=$(hostname -I | tr ' ' '\n' | grep -E '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$')
  for ip in $ip_addresses; do
    subnet=$(echo $ip | awk -F. '{print "'http://" $1 "." $2 "." $3 ".*'"}')
    echo $subnet
  done
}

# Function to detect the domain name of the device 
detect_systems_domainname() {
  domain_name=$(hostname -d)
  if [ -z "$domain_name" ]; then
    #no domain name set
    return
  else
    echo "http://*.$domain_name"
  fi
}

#function to retrieve IPs from fppd discovered multisync devices
extract_fppd_multisync_ips() {
  fppd_json_data=$(curl -s "http://localhost/api/fppd/multiSyncSystems")

readarray -t systems < <(echo "$fppd_json_data" | jq -c '.systems[]')

# Print each system from the array
for system in "${systems[@]}"; do
  ip=$(echo "$system" | jq -r '.address')
  type=$(echo "$system" | jq -r '.type')

      case $type in
      "ESPixelStick-ESP8266")
        echo "ws://$ip"
        ;;
      *)
        echo "http://$ip"
        ;;
    esac
done
}

#function to extract multisync IPs from settings file
# Function to extract IPv4 addresses from a file
extract_multisync_ips() {
  file_path="$MEDIADIR/settings"
  multisync_ips=$(grep -E "MultiSyncRemotes|MultiSyncExtraRemotes" "$file_path" | grep -Eo '([0-9]{1,3}\.){3}[0-9]{1,3}' | sort -u)
  for ip in $multisync_ips; do
    echo "http://$ip"
  done
}

# Function to generate the CSP header
generate_csp() {
    # Initialize an associative array to store the combined values
    declare -A combined_values

    # Include default values
    for key in "${!DEFAULT_VALUES[@]}"; do
        combined_values[$key]="${DEFAULT_VALUES[$key]}"
    done

    # Read current CSP values from JSON file and combine them with default values
    while IFS= read -r entry; do
        key=$(echo "$entry" | awk '{print $1}')
        value=$(echo "$entry" | awk '{$1=""; print $0}' | xargs)
        
        if [ -n "${combined_values[$key]}" ]; then
            combined_values[$key]="${combined_values[$key]} $value"
        else
            combined_values[$key]="$value"
        fi
    done < <(jq -r 'to_entries | map("\(.key) \(.value | join(" "))") | .[]' $JSON_FILE)

    # Detect cape domains and add them to default-src
    cape_domains=$(detectCapeDomains)
    if [ -n "$cape_domains" ]; then
        combined_values["img-src"]="${combined_values["img-src"]} $cape_domains"
        # combined_values["connect-src"]="${combined_values["connect-src"]} $cape_domains"
    fi

    # Detect FPPD Discovered MultiSync Hosts to trust
    #fppd_multisync_ips=$(extract_fppd_multisync_ips)
    #if [ -n "$fppd_multisync_ips" ]; then
    #    combined_values["connect-src"]="${combined_values["connect-src"]} $fppd_multisync_ips"
    #fi

    # Detect MultiSync (Hard coded) Hosts to trust
    #multisync_ips=$(extract_multisync_ips)
    #if [ -n "$multisync_ips" ]; then
    #    combined_values["connect-src"]="${combined_values["connect-src"]} $multisync_ips"
    #fi
    #combined_values["connect-src"]="* ${combined_values["connect-src"]}"


    # Detect IP subnets to trust and add them to connect-src
   #subnets=$(detect_systems_subnets)
    #if [ -n "$subnets" ]; then
    #    combined_values["connect-src"]="${combined_values["connect-src"]} $subnets"
    #fi

    # Detect domain of local device
    #local_domain=$(detect_systems_domainname)
    #if [ -n "$local_domain" ]; then
    #    combined_values["connect-src"]="${combined_values["connect-src"]} $local_domain"
    #fi

    # Remove duplicate values for each key
    for key in "${!combined_values[@]}"; do
        combined_values[$key]=$(echo "${combined_values[$key]}" | tr ' ' '\n' | sort -u | tr '\n' ' ')
    done

    # Construct the CSP_HEADER from combined values
    CSP_HEADER=""
    for key in "${!combined_values[@]}"; do
        CSP_HEADER="$CSP_HEADER; $key ${combined_values[$key]}"
    done

    # Ensure CSP_HEADER is trimmed correctly
    CSP_HEADER=$(echo "$CSP_HEADER" | sed 's/^ *; *//; s/ *; *$//')
    # echo "Final CSP_HEADER: $CSP_HEADER"

    cat <<EOL > $FPPDIR/etc/apache2.csp
<IfModule mod_headers.c>
    Header set Content-Security-Policy "$CSP_HEADER"
</IfModule>
EOL
    echo "CSP header generated."
}

# Function to echo current JSON settings
echo_current_settings() {
    jq . $JSON_FILE
}

# Parse arguments
case "$1" in
    add)
        add_domain "$2" "$3"
        generate_csp
        gracefullyReloadApacheConf
        ;;
    remove)
        remove_domain "$2" "$3"
        generate_csp
        gracefullyReloadApacheConf
        ;;
    show)
        echo_current_settings
        ;;
    regenerate)
        generate_csp
        gracefullyReloadApacheConf
        ;;
    regenerate-norestart)
        generate_csp
        ;;        
    *)
        echo "Usage: $0 {add|remove} key domain"
        echo "Possible Keys are: 'default-src','connect-src', 'img-src','script-src','style-src','object-src'"
        echo "To view current config:"
        echo "Usage: $0 {show}"
        echo "To regenerate current config without making changes:"
        echo "Usage: $0 {regenerate}"
        exit 1
        ;;
esac
