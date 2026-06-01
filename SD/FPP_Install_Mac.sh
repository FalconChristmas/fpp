#!/bin/bash

ARCH=$(uname -p)
MEDIADIR=$(pwd)

if [ "${ARCH}" == "arm" ]; then
    BREWLOC="/opt/homebrew"
else
    BREWLOC="/usr/local"
fi

# Snapshot a config file the first time we modify it so FPP_Uninstall_Mac.sh can
# restore the pristine original. We only snapshot once: re-running the installer
# must not overwrite an existing backup with an already-modified file.
backup_orig() {
    if [ -f "$1" ] && [ ! -f "$1.fpp-orig" ]; then
        cp -p "$1" "$1.fpp-orig"
    fi
}

echo "FPP macOS Installation"
echo ""
echo "Welcome to the FPP install script for macOS.   Installing FPP on a Mac is"
echo "a two step process:"
echo ""
echo "Step 1: Install dependencies. FPP uses a bunch of libraries and utilities"
echo "        that are not available by default on macOS.  We use 'homebrew' to"
echo "        to install and manage those dependencies."
echo ""
echo "Step 2: Install and configure FPP.  This scrips should be run from a"
echo "        directory to act as the 'media' directory for FPP.  This is where"
echo "        FPP stores all its configuration, the sequences/music, plugins,"
echo "        scripts, effects, etc..."
echo "        The current directory is: ${MEDIADIR}"
echo ""
echo -n "Do you wish to proceed? [N/y] "
read ANSWER
if [ "x${ANSWER}" != "xY" -a "x${ANSWER}" != "xy" ]; then
    echo
    echo "Install cancelled."
    echo
    exit
fi
echo ""
if [ -x "${BREWLOC}/bin/brew" ] && "${BREWLOC}/bin/brew" --version >/dev/null 2>&1; then
    echo "Homebrew is already installed at ${BREWLOC} ($(${BREWLOC}/bin/brew --version | head -1))."
    echo "Skipping Homebrew installation."
else
    echo "The first thing that needs to be installed is 'homebrew' from https://brew.sh/ "
    echo "This requires sudo and will ask for your password to install itself."
    echo -n "Do you wish to proceed? [N/y] "
    read ANSWER
    if [ "x${ANSWER}" != "xY" -a "x${ANSWER}" != "xy" ]; then
        echo
        echo "Install cancelled."
        echo
        exit
    fi
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

NEEDBREW=1
if [ -f ~/.zprofile ]; then
    grep -q brew ~/.zprofile
    NEEDBREW=$?
fi
if [ $NEEDBREW != 0 ]; then
if [ "${ARCH}" == "arm" ]; then
    echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
else
    echo 'eval "$(/usr/local/bin/brew shellenv)"' >> ~/.zprofile
fi
fi
if [ "${ARCH}" == "arm" ]; then
    eval "$(/opt/homebrew/bin/brew shellenv)"
else
    eval "$(/usr/local/bin/brew shellenv)"
fi
echo ""
echo "The next step is to use brew to install several needed dependencies.   This includes"
echo "   php, git, httpd, ccache, make, zstd, wget, taglib, mosquitto,"
echo "   jsoncpp, drogon, graphicsmagick, libusb, sdl3, ffmpeg"
echo ""
echo -n "Do you wish to proceed? [N/y] "
read ANSWER
if [ "x${ANSWER}" != "xY" -a "x${ANSWER}" != "xy" ]; then
    echo
    echo "Install cancelled."
    echo
    exit
fi
# NOTE: macOS audio goes through CoreAudio; PipeWire and GStreamer are Linux-only
# paths here, so they are intentionally NOT installed. Video playback uses
# GStreamer, which is currently gated off on macOS (the build keys on a Linux
# header path) and whose video output is built on DRM/KMS (kmssink) with no macOS
# equivalent. Getting video working on Mac is future work: it needs the build to
# detect GStreamer via pkg-config and GStreamerOut.cpp's DRM/kmssink path
# decoupled behind a Linux guard plus a macOS video sink -- not just adding the
# gstreamer brew packages here. See fpp_so.mk and src/mediaoutput/GStreamerOut.cpp.
brew install php git httpd ccache make zstd wget taglib mosquitto jsoncpp drogon graphicsmagick libusb sdl3 ffmpeg
echo ""
ccache -M 350M
ccache --set-config=temporary_dir=/tmp
ccache --set-config=sloppiness=pch_defines,time_macros
echo ""
echo "The dependencies are now installed.   We will now proceed to setup FPP."
echo "We will now clone FPP into the current directory and create a bunch of "
echo "subdirectories to store configuration, plugins, etc...   We will also"
echo "build FPPD and configure the HTTP server."
echo ""
echo -n "Do you wish to proceed? [N/y] "
read ANSWER
if [ "x${ANSWER}" != "xY" -a "x${ANSWER}" != "xy" ]; then
    echo
    echo "Install cancelled."
    echo
    exit
fi
echo "Creating directories"
mkdir -p backups
mkdir -p cache
mkdir -p config
mkdir -p effects
mkdir -p images
mkdir -p logs
mkdir -p music
mkdir -p playlists
mkdir -p plugindata
mkdir -p plugins
mkdir -p scripts
mkdir -p sequences
mkdir -p tmp
mkdir -p upload
mkdir -p videos
mkdir -p virtualdisplay_assets
echo "Cloning FPP"
git clone https://github.com/FalconChristmas/fpp fpp
echo "Building FPP"
cd fpp/src
make -j 4
cd ../www
echo "${MEDIADIR}" > media_root.txt
cd ../..
echo "Configuring HTTP"
HTTPCONF="${BREWLOC}/etc/httpd/httpd.conf"
FPPHTTPCONF="${BREWLOC}/etc/httpd/extra/fpp.conf"
USER=$(whoami)

# Two changes cannot be made from an Include and must be edited in the base
# config, so we snapshot it first for a clean restore on uninstall:
#   * switch the listen port to 80 (an Include can only *add* a Listen, which
#     would leave the original :8080 bound as well)
#   * switch the MPM from prefork to event (only one MPM may be loaded, so this
#     cannot be done additively). Patterns are anchored so re-runs are no-ops.
backup_orig "$HTTPCONF"
sed -i '' -e "s/^Listen 8080.*/Listen [::]:80/" -e "s/^Listen 80$/Listen [::]:80/" $HTTPCONF
sed -i '' -e "s+^#LoadModule mpm_event+LoadModule mpm_event+" $HTTPCONF
sed -i '' -e "s+^LoadModule mpm_prefork+#LoadModule mpm_prefork+" $HTTPCONF
grep -q "extra/fpp.conf" "$HTTPCONF" || echo "IncludeOptional ${FPPHTTPCONF}" >> "$HTTPCONF"

# The config is built in two parts: a small macOS "glue" prefix (module loading,
# User/Group, the php-fpm handler -- the bits Debian supplies out-of-band) and
# then the canonical etc/apache2.site with its paths rewritten for this install.
# Uninstall just deletes this whole file (plus the Include line above).
cat > "$FPPHTTPCONF" <<EOF
# FPP (Falcon Player) web server configuration.
# Created by FPP_Install_Mac.sh -- delete this file (and the matching
# "IncludeOptional .../extra/fpp.conf" line in httpd.conf) to disable it.

# Part 1 -- macOS glue. Homebrew's httpd has no Debian-style mods-enabled, so
# load the modules etc/apache2.site relies on (guards make re-runs no-ops).
<IfModule !proxy_module>
    LoadModule proxy_module lib/httpd/modules/mod_proxy.so
</IfModule>
<IfModule !proxy_http_module>
    LoadModule proxy_http_module lib/httpd/modules/mod_proxy_http.so
</IfModule>
<IfModule !proxy_fcgi_module>
    LoadModule proxy_fcgi_module lib/httpd/modules/mod_proxy_fcgi.so
</IfModule>
<IfModule !proxy_wstunnel_module>
    LoadModule proxy_wstunnel_module lib/httpd/modules/mod_proxy_wstunnel.so
</IfModule>
<IfModule !xml2enc_module>
    LoadModule xml2enc_module lib/httpd/modules/mod_xml2enc.so
</IfModule>
<IfModule !proxy_html_module>
    LoadModule proxy_html_module lib/httpd/modules/mod_proxy_html.so
</IfModule>
<IfModule !rewrite_module>
    LoadModule rewrite_module lib/httpd/modules/mod_rewrite.so
</IfModule>
<IfModule !expires_module>
    LoadModule expires_module lib/httpd/modules/mod_expires.so
</IfModule>
<IfModule !cgid_module>
    LoadModule cgid_module lib/httpd/modules/mod_cgid.so
</IfModule>

# Link declarations the ProxyHTML directives in apache2.site's /proxy block
# need. Stock httpd.conf only includes these once proxy_html is loaded (above).
<IfModule proxy_html_module>
    Include ${BREWLOC}/etc/httpd/extra/proxy-html.conf
</IfModule>

User ${USER}
Group staff
ServerName localhost

<IfModule dir_module>
    DirectoryIndex index.php index.html
</IfModule>

<IfModule setenvif_module>
SetEnvIfNoCase ^Authorization$ "(.+)" HTTP_AUTHORIZATION=\$1
</IfModule>
<FilesMatch ".+\.ph(ar|p|tml)$">
    SetEnv proxy-sendchunked
    SetHandler "proxy:fcgi://127.0.0.1:9000"
</FilesMatch>
<FilesMatch ".+\.phps$">
    # Deny access to raw php sources by default
    Require all denied
</FilesMatch>
# Deny access to files without filename (e.g. '.php')
<FilesMatch "^\.ph(ar|p|ps|tml)$">
    Require all denied
</FilesMatch>
EOF

# Part 2 -- the canonical site config shared with Linux. We substitute the
# install paths and drop the one Debian-only Include (serve-cgi-bin.conf, which
# is a hard Include of a path that does not exist on macOS). Appending
# etc/apache2.site verbatim keeps the macOS and Linux web routing in lockstep
# instead of maintaining a divergent copy here.
sed -e "s#/opt/fpp#${MEDIADIR}/fpp#g" \
    -e "s#/home/fpp/media#${MEDIADIR}#g" \
    -e "/conf-available\/serve-cgi-bin.conf/d" \
    "${MEDIADIR}/fpp/etc/apache2.site" >> "$FPPHTTPCONF"

# Detect the installed PHP version dynamically. Homebrew's "php" formula tracks
# the latest stable (8.5+ as of 2025), so a hardcoded list goes stale fast.
# Prefer the version of the linked "php" binary, falling back to the highest
# version directory present under etc/php.
PHP_VERSION=$(php -r 'echo PHP_MAJOR_VERSION.".".PHP_MINOR_VERSION;' 2>/dev/null)
if [ -z "${PHP_VERSION}" ] || [ ! -d "${BREWLOC}/etc/php/${PHP_VERSION}" ]; then
    PHP_VERSION=$(ls "${BREWLOC}/etc/php" 2>/dev/null | sort -V | tail -1)
fi
if [ -z "${PHP_VERSION}" ]; then
    echo "WARNING: Could not locate a Homebrew PHP install under ${BREWLOC}/etc/php;"
    echo "         skipping PHP configuration. FPP uploads/timeouts may be misconfigured."
fi


if [ -n "${PHP_VERSION}" ]; then
echo "Configuring PHP"
PHPCONFD="${BREWLOC}/etc/php/${PHP_VERSION}/conf.d"
PHPFPMD="${BREWLOC}/etc/php/${PHP_VERSION}/php-fpm.d"

# php.ini settings go in a drop-in that PHP scans automatically after php.ini,
# so php.ini itself is never modified. Uninstall just deletes this file.
mkdir -p "$PHPCONFD"
cat > "${PHPCONFD}/fpp.ini" <<EOF
; FPP (Falcon Player) PHP overrides.
; Created by FPP_Install_Mac.sh -- delete to revert to php.ini defaults.
max_execution_time = 1000
max_input_time = 900
max_input_vars = 5000
post_max_size = 4G
upload_max_filesize = 4G
upload_tmp_dir = ${MEDIADIR}/upload
default_socket_timeout = 900
short_open_tag = On
output_buffering = 1024
EOF

# php-fpm pools can't be layered across files, so we ship a dedicated FPP pool
# and disable the stock "www" pool by moving it aside (restored on uninstall).
# user/group are intentionally omitted: php-fpm runs non-root here (started by
# "brew services" as the current user) and would only warn if they were set.
mkdir -p "$PHPFPMD"
if [ -f "${PHPFPMD}/www.conf" ] && [ ! -f "${PHPFPMD}/www.conf.fpp-off" ]; then
    mv "${PHPFPMD}/www.conf" "${PHPFPMD}/www.conf.fpp-off"
fi
cat > "${PHPFPMD}/fpp.conf" <<EOF
; FPP (Falcon Player) php-fpm pool.
; Created by FPP_Install_Mac.sh. Replaces the stock "www" pool (moved aside to
; www.conf.fpp-off). Delete this file and restore www.conf to revert.
[www]
listen = 127.0.0.1:9000
pm = dynamic
pm.max_children = 25
pm.start_servers = 2
pm.min_spare_servers = 1
pm.max_spare_servers = 3
clear_env = no
env[PATH] = ${BREWLOC}/bin:${BREWLOC}/sbin:/usr/bin:/bin:/usr/sbin:/sbin
EOF
fi


echo "Configuring FPPD"
mkdir -p ~/Library/LaunchAgents
cp fpp/etc/macOS/falconchristmas.fppd.plist ~/Library/LaunchAgents
sed -i '' -e "s+FPPDIR+${MEDIADIR}/fpp+g" ~/Library/LaunchAgents/falconchristmas.fppd.plist
sed -i '' -e "s+MEDIADIR+${MEDIADIR}+g" ~/Library/LaunchAgents/falconchristmas.fppd.plist
# Load and start the agent. Remove any previous instance first (both the modern
# and legacy ways), ignoring errors. Then prefer the modern bootstrap, but fall
# back to the legacy "load -w" if the GUI domain rejects it -- bootstrap into
# gui/<uid> can fail with "Bootstrap failed: 5: Input/output error" when run
# without an Aqua session (e.g. over SSH) or while launchd is still tearing down
# a just-removed instance. RunAtLoad starts fppd either way.
FPPD_PLIST="${HOME}/Library/LaunchAgents/falconchristmas.fppd.plist"
FPPD_DOMAIN="gui/$(id -u)"
launchctl bootout "${FPPD_DOMAIN}/falconchristmas.fppd" 2>/dev/null
launchctl unload "${FPPD_PLIST}" 2>/dev/null
if launchctl bootstrap "${FPPD_DOMAIN}" "${FPPD_PLIST}" 2>/dev/null; then
    launchctl kickstart -k "${FPPD_DOMAIN}/falconchristmas.fppd" 2>/dev/null
else
    echo "  launchctl bootstrap was rejected; using legacy 'launchctl load' instead"
    launchctl load -w "${FPPD_PLIST}"
fi

echo "Starting HTTP"
brew services start php
# httpd is configured to listen on port 80, which requires root to bind, so it
# must run in the system (root) domain via sudo rather than the user domain.
sudo brew services start httpd

echo "Installation complete.  You should now be able to point your browser at http://localhost to use to FPP."
