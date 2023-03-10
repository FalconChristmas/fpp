#!/bin/bash

ARCH=$(uname -p)
MEDIADIR=$(pwd)

if [ "${ARCH}" == "arm" ]; then
    BREWLOC="/opt/homebrew"
else
    BREWLOC="/usr/local"
fi

export PHP_VERSION=8.2


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
echo "   php, git, httpd, ffmpeg, ccache, make, sdl2, zstd, wget, taglib, mosquitto,"
echo "   jsoncpp, libhttpserver, graphicsmagick"
echo ""
echo -n "Do you wish to proceed? [N/y] "
read ANSWER
if [ "x${ANSWER}" != "xY" -a "x${ANSWER}" != "xy" ]; then
    echo
    echo "Install cancelled."
    echo
    exit
fi
brew install php git httpd ffmpeg ccache make sdl2 zstd wget taglib mosquitto jsoncpp libhttpserver graphicsmagick libusb
echo ""
ccache -M 350M
ccache --set-config=temporary_dir=/tmp
ccache --set-config=sloppiness=pch_defines,time_macros
echo ""
echo "The dependencies are now installed.   We will not proceed to setup FPP."
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
echo "Cloning FPP"
git clone https://github.com/FalconChristmas/fpp fpp
echo "Building FPP"
cd fpp/src
make -j 4
cd ../www
echo "${MEDIADIR}" > media_root.txt
ln -f -s "${MEDIADIR}/config/.htaccess" .htaccess
cd proxy
ln -f -s "${MEDIADIR}/config/proxies" .htaccess
cd ../../..
echo "Configuring HTTP"
HTTPCONF="${BREWLOC}/etc/httpd/httpd.conf"
USER=$(whoami)
sed -i -e "s/Listen 8080.*/Listen 80/" $HTTPCONF
sed -i -e "s+#LoadModule proxy+LoadModule proxy+g" $HTTPCONF
sed -i -e "s+LoadModule proxy_balanc+#LoadModule proxy_balanc+g" $HTTPCONF
sed -i -e "s+#LoadModule rewrite+LoadModule rewrite+g" $HTTPCONF
sed -i -e "s+#LoadModule watchdog+LoadModule watchdog+g" $HTTPCONF
sed -i -e "s+#LoadModule mpm_event+LoadModule mpm_event+g" $HTTPCONF
sed -i -e "s+LoadModule mpm_prefork+#LoadModule mpm_prefork+g" $HTTPCONF
sed -i -e "s+User .*+User ${USER}+g" $HTTPCONF
sed -i -e "s+Group .*+Group staff+g" $HTTPCONF
sed -i -e "s+DirectoryIndex index.*+DirectoryIndex index.php index.html+g" $HTTPCONF
sed -i -e "s+${BREWLOC}/var/www+${MEDIADIR}/fpp/www+g" $HTTPCONF
sed -i -e "s+${BREWLOC}/var/log/httpd/+${MEDIADIR}/logs/apache2-+g" $HTTPCONF
sed -i -e "s/AllowOverride None/AllowOverride All/1" $HTTPCONF

cat >> $HTTPCONF <<EOF
ServerName localhost

<IfModule setenvif_module>
SetEnvIfNoCase ^Authorization$ "(.+)" HTTP_AUTHORIZATION=\$1
</IfModule>
<FilesMatch ".+\.ph(ar|p|tml)$">
    SetEnv proxy-sendchunked
    SetHandler "proxy:fcgi://127.0.0.1:9000"
</FilesMatch>
<FilesMatch ".+\.phps$">
    # Deny access to raw php sources by default
    # To re-enable it's recommended to enable access to the files
    # only in specific virtual host or directory
    Require all denied
</FilesMatch>
# Deny access to files without filename (e.g. '.php')
<FilesMatch "^\.ph(ar|p|ps|tml)$">
    Require all denied
</FilesMatch>
EOF


echo "Configuring PHP"
PHPCONF="${BREWLOC}/etc/php/${PHP_VERSION}/php.ini"
PHPPOOLCONF="${BREWLOC}/etc/php/${PHP_VERSION}/php-fpm.d/www.conf"
sed -i -e "s/^max_execution_time =.*/max_execution_time = 1000/g" $PHPCONF
sed -i -e "s/^max_input_time =.*/max_input_time = 900/g" $PHPCONF
sed -i -e "s/^.*max_input_vars =.*/max_input_vars = 5000/g" $PHPCONF
sed -i -e "s/^post_max_size =.*/post_max_size = 4G/g" $PHPCONF
sed -i -e "s/^upload_max_filesize =.*/upload_max_filesize = 4G/g" $PHPCONF
sed -i -e "s+^.*upload_tmp_dir =.*+upload_tmp_dir = ${MEDIADIR}/upload+g" $PHPCONF
sed -i -e "s/^default_socket_timeout =.*/default_socket_timeout = 900/g" $PHPCONF
sed -i -e "s/^short_open_tag =.*/short_open_tag = On/g" $PHPCONF
sed -i -e "s/^output_buffering.*/output_buffering = 1024/" $PHPCONF

sed -i -e "s+user .*+; user ${USER}+g" $PHPPOOLCONF
sed -i -e "s+group .*+; group staff+g" $PHPPOOLCONF
sed -i -e "s+pm.max_children .*+pm.max_children = 25+g" $PHPPOOLCONF
sed -i -e "s+^;clear_env.*+clear_env = no+g" $PHPPOOLCONF

cat >> $PHPPOOLCONF <<EOF

env[PATH] = /opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/System/Cryptexes/App/usr/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Library/Apple/usr/bin:

EOF


echo "Configuring FPPD"
mkdir -p ~/Library/LaunchAgents
cp fpp/etc/macOS/falconchristmas.fppd.plist ~/Library/LaunchAgents
sed -i -e "s+FPPDIR+${MEDIADIR}/fpp+g" ~/Library/LaunchAgents/falconchristmas.fppd.plist
sed -i -e "s+MEDIADIR+${MEDIADIR}+g" ~/Library/LaunchAgents/falconchristmas.fppd.plist
launchctl load -w ~/Library/LaunchAgents/falconchristmas.fppd.plist
launchctl start falconchristmas.fppd

echo "Starting HTTP"
brew services start httpd

echo "Installation complete.  You should now be able to point your browser at http://localhost to use to FPP."
