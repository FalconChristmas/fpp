#!/bin/bash
##############################################################################
# Some internal setup
PATH="/sbin:/bin:/usr/sbin:/usr/bin"

BINDIR=$(cd $(dirname $0) && pwd)

. ${BINDIR}/common
. ${BINDIR}/functions

#############################################################################
# Path to your JSON default structure file
JSON_STRUCT=$FPPDIR"/etc/csp_allowed_default_struct.json"
# Path to your local CSP JSON override file
JSON_FILE=$MEDIADIR"/config/csp_allowed_domains.json"

# Default key values hard-coded to include in all configs
declare -A DEFAULT_VALUES
DEFAULT_VALUES=( 
    ["default-src"]="'self' http://www.w3.org"
    ["connect-src"]="'self' https://raw.githubusercontent.com https://kulplights.com https://www.hansonelectronics.com.au https://www.wiredwatts.com"
    ["object-src"]="'none' "
    ["img-src"]="'self' blob: data: http://www.w3.org https://www.paypal.com https://www.paypalobjects.com https://kulplights.com https://www.hansonelectronics.com.au https://www.wiredwatts.com"
    ["script-src"]="'self' 'unsafe-inline' 'unsafe-eval' https://api.falconplayer.com"
    ["style-src"]="'self' 'unsafe-inline'"
)

# Generate local CSP JSON override file if NOT exists
if [ ! -f $JSON_FILE ]; then
    cp $JSON_STRUCT $JSON_FILE
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
        combined_values["connect-src"]="${combined_values["connect-src"]} $cape_domains"
    fi

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