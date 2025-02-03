# Apache CSP (Content Security Policy)

In order to help reduce security threats fpp has CSP enabled for Apache2

This allows us to define which domains are trusted to pull in content of various types - images, scripts etc..

When developing the core fpp application the mechanism to define default trusted sites is to add them to the script which controls the CSP creation in:

`/opt/fpp/scripts/ManageApacheContentPolicy.sh`

For pluggins to be able to trust particular domains they need to add them to the local configuration on installation by calling this script with arguments, here's an example:

in the pluggins fpp_install.sh file:

```
# Include common scripts functions and variables
. ${FPPDIR}/scripts/common

# Add required Apache CSP (Content-Security-Policy allowed domains
${FPPDIR}/scripts/ManageApacheContentPolicy.sh add connect-src https://domaintotrust.co.uk
${FPPDIR}/scripts/ManageApacheContentPolicy.sh add img-src https://anotherdomain.com

# Need to force reboot for CSP change to take affect
setSetting rebootFlag 1
```

Possible Keys are: 'default-src', 'connect-src', 'img-src', 'script-src', 'style-src', 'object-src'

## Key Definitions:

1. default-src - catchall which would completely trust a domain
2. connect-src - defines domains which are allowed to provide xhr data
3. img-src - defines domains which are allowed to provide images
4. script-src - defines domains which are allowed to provide script content
5. style-src - defines domains which are allowed to provide style content
6. object-src - defines domains which are allowed to provide object content
