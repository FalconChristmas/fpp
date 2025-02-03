#!/bin/bash
##############################################################################
# Some internal setup
PATH="/sbin:/bin:/usr/sbin:/usr/bin"

BINDIR=$(cd $(dirname $0) && pwd)

. ${BINDIR}/common
. ${BINDIR}/functions

#############################################################################
# Path to your JSON file
JSON_FILE=$FPPDIR"/etc/csp_allowed_domains.json"

# Default key values hard-coded to include in all configs
declare -A DEFAULT_VALUES
DEFAULT_VALUES=( 
    ["default-src"]="'self' http://www.w3.org"
    ["connect-src"]="'self' https://raw.githubusercontent.com https://kulplights.com https://www.hansonelectronics.com.au https://www.wiredwatts.com"
    ["object-src"]="'none' "
    ["img-src"]="'self' blob: http://www.w3.org https://www.paypal.com https://www.paypalobjects.com"
    ["script-src"]="'self' 'unsafe-inline' https://api.falconplayer.com"
    ["style-src"]="'self' 'unsafe-inline'"
)

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
        ;;
    remove)
        remove_domain "$2" "$3"
        generate_csp
        ;;
    show)
        echo_current_settings
        ;;
    regenerate)
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