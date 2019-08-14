#!/bin/bash

#############################################################################
# FPP Install Script
#
# This is a quick and dirty script to take a stock OS build and install FPP
# onto it for use in building FPP images or setting up a development system.
#
# The script will automatically setup the system for the development
# version of FPP, the system can then be switched to a new version number
# or git branch for release.
#
#############################################################################
# To use this script, download the latest copy from github and run it as
# root on the system where you want to install FPP:
#
# wget -O ./FPP_Install.sh https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/FPP_Install.sh
# chmod 700 ./FPP_Install.sh
# su
# ./FPP_Install.sh
#
#############################################################################
# NOTE: This script is used to build the SD images for FPP releases.  Its
#       main goal is to prep these release images.  It may be used for othe
#       purposes, but it is not our main priority to keep FPP working on
#       other Linux distributions or distribution versions other than those
#       we base our FPP releases on.  Currently, FPP images are based on the
#       following OS images for the Raspberry Pi and BeagleBone Black:
#
#       Raspberry Pi
#           - URL: https://www.raspberrypi.org/downloads/
#           - Image
#             - 2017-11-29-raspbian-stretch-lite.zip
#           - Login/Password
#             - pi/raspberry
#
#       BeagleBone Black
#           - URL: http://beagleboard.org/latest-images
#           - Images
#             - bone-debian-9.3-iot-armhf-2018-03-05-4gb.img
#           - Login/Password
#             - debian/temppwd
#
#       Other OS images may work with this install script and FPP on the
#       Pi and BBB platforms, but these are the images we are currently
#       targetting for support.
#
#############################################################################
# Other platforms should be functioning:
#
#       ODROID C1
#           http://oph.mdrjr.net/meveric/images/
#           - Jessie/Debian-Jessie-1.0-20160131-C1.img.xz
#           - Login/Password
#             - root/odroid
#           - When building a FPP image from this stock image, we will need
#             to do something about the image size so we don't bloat the
#             the FPP download.
#
#       Pine64
#           http://wiki.pine64.org/index.php/Pine_A64_Software_Release
#           - "Debian Base (3.10.102 BSP 2)" image
#           - Login/Password
#             - debian/debian
#           - This image has very little free space after installing FPP,
#             so you may want to expand the root partition by a few hundred MB.
#
#############################################################################
SCRIPTVER="1.0"
FPPBRANCH=${FPPBRANCH:-"master"}
FPPIMAGEVER="3.0"
FPPCFGVER="47"
FPPPLATFORM="UNKNOWN"
FPPDIR=/opt/fpp
FPPUSER=fpp
FPPHOME=/home/${FPPUSER}
OSVER="UNKNOWN"

#############################################################################
# Some Helper Functions
#############################################################################
# Check local time against U.S. Naval Observatory time, set if too far out.
# This function was copied from /opt/fpp/scripts/functions in FPP source
# since we don't have access to the source until we clone the repo.
checkTimeAgainstUSNO () {
	# www.usno.navy.mil is not pingable, so check github.com instead
	ping -q -c 1 github.com > /dev/null 2>&1

	if [ $? -eq 0 ]
	then
		echo "FPP: Checking local time against U.S. Naval Observatory"

		# allow clocks to differ by 24 hours to handle time zone differences
		THRESHOLD=86400
		USNOSECS=$(wget -q -O - http://www.usno.navy.mil/cgi-bin/time.pl | sed -e "s/.*\">//" -e "s/<\/t.*//" -e "s/...$//")

        if [ "x${USNOSECS}" != "x" ]
        then

            LOCALSECS=$(date +%s)
            MINALLOW=$(expr ${USNOSECS} - ${THRESHOLD})
            MAXALLOW=$(expr ${USNOSECS} + ${THRESHOLD})

            #echo "FPP: USNO Secs  : ${USNOSECS}"
            #echo "FPP: Local Secs : ${LOCALSECS}"
            #echo "FPP: Min Valid  : ${MINALLOW}"
            #echo "FPP: Max Valid  : ${MAXALLOW}"

            echo "FPP: USNO Time  : $(date --date=@${USNOSECS})"
            echo "FPP: Local Time : $(date --date=@${LOCALSECS})"

            if [ ${LOCALSECS} -gt ${MAXALLOW} -o ${LOCALSECS} -lt ${MINALLOW} ]
            then
                echo "FPP: Local Time is not within 24 hours of USNO time, setting to USNO time"
                date $(date --date="@${USNOSECS}" +%m%d%H%M%Y.%S)

                LOCALSECS=$(date +%s)
                echo "FPP: New Local Time: $(date --date=@${LOCALSECS})"
            else
                echo "FPP: Local Time is OK"
            fi
		else
			echo "FPP: Incorrect result or timeout from query to U.S. Naval Observatory"
        fi
    else
		echo "FPP: Not online, unable to check time against U.S. Naval Observatory."
	fi
}

#############################################################################
# Gather some info about our system
. /etc/os-release
OSID=${ID}
OSVER="${ID}_${VERSION_ID}"

if [ "x${PRETTY_NAME}" = "x" ]
then
	echo "ERROR: /etc/os-release does not exist, will not install"
	exit
fi

if [ "x${OSID}" = "x" ]
then
	echo "ERROR: /etc/os-release does not contain OS ID, will not install"
	exit
fi

# Parse build options as arguments
build_ola=false
build_omxplayer=false
while [ -n "$1" ]; do
	case $1 in
		--build-ola)
			build_ola=true
			shift
			;;
		--build-omxplayer)
			build_omxplayer=true
			shift
			;;
		*)
			echo "Unknown option $1" >&2
			exit 1
			;;
	esac
done

# Attempt to detect the platform we are installing on
if [ "x${OSID}" = "xraspbian" ]
then
	FPPPLATFORM="Raspberry Pi"
	OSVER="debian_${VERSION_ID}"
elif [ "x${VARIANT}" = "xDebian on C.H.I.P" ]; then
	FPPPLATFORM="CHIP"
elif [ -e "/sys/class/leds/beaglebone:green:usr0" ]
then
	FPPPLATFORM="BeagleBone Black"
elif [ ! -z "$(grep ODROIDC /proc/cpuinfo)" ]
then
	FPPPLATFORM="ODROID"
elif [ ! -z "$(grep sun50iw1p1 /proc/cpuinfo)" ]
then
	FPPPLATFORM="Pine64"
elif [ ! -z "$(grep sun8i /proc/cpuinfo)" ]
then
	FPPPLATFORM="OrangePi"
elif [ "x${OSID}" = "xdebian" ]
then
	FPPPLATFORM="Debian"
else
	FPPPLATFORM="UNKNOWN"
fi

checkTimeAgainstUSNO

#############################################################################
echo "============================================================"
echo "$0 v${SCRIPTVER}"
echo ""
echo "FPP Image Version: v${FPPIMAGEVER}"
echo "FPP Directory    : ${FPPDIR}"
echo "FPP Branch       : ${FPPBRANCH}"
echo "Operating System : ${PRETTY_NAME}"
echo "Platform         : ${FPPPLATFORM}"
echo "OS Version       : ${OSVER}"
echo "Build OLA        : $build_ola"
if [ "x${FPPPLATFORM}" = "xRaspberry Pi" ]; then
echo "Build omxplayer  : $build_omxplayer"
fi
echo "============================================================"
#############################################################################

echo ""
echo "Notes:"
echo "- Does this system have internet access to install packages and FPP?"
echo ""
echo "WARNINGS:"
echo "- This install expects to be run on a clean freshly-installed system."
echo "  The script is not currently designed to be re-run multiple times."
echo "- This installer will take over your system.  It will disable any"
echo "  existing 'pi' or 'debian' user and create a '${FPPUSER}' user.  If the system"
echo "  has an empty root password, remote root login will be disabled."
echo ""

echo -n "Do you wish to proceed? [N/y] "
read ANSWER
if [ "x${ANSWER}" != "xY" -a "x${ANSWER}" != "xy" ]
then
	echo
	echo "Install cancelled."
	echo
	exit
fi

STARTTIME=$(date)

#######################################
# Log output and notify user
echo "ALL output will be copied to FPP_Install.log"
exec > >(tee -a FPP_Install.log)
exec 2>&1
echo "========================================================================"
echo "FPP_Install.sh started at ${STARTTIME}"
echo "------------------------------------------------------------------------"

#######################################
# Remove old /etc/fpp if it exists
if [ -e "/etc/fpp" ]
then
	echo "FPP - Removing old /etc/fpp"
	rm -rf /etc/fpp
fi

#######################################
# Create /etc/fpp directory and contents
echo "FPP - Creating /etc/fpp and contents"
mkdir /etc/fpp
echo "${FPPPLATFORM}" > /etc/fpp/platform
echo "v${FPPIMAGEVER}" > /etc/fpp/rfs_version
echo "${FPPCFGVER}" > /etc/fpp/config_version

#######################################
# Setting hostname
echo "FPP - Setting hostname"
echo "FPP" > /etc/hostname
hostname FPP

#######################################
# Add FPP hostname entry
echo "FPP - Adding 'FPP' hostname entry"
# Remove any existing 127.0.1.1 entry first
sed -i -e "/^127.0.1.1[^0-9]/d" /etc/hosts
echo "127.0.1.1       FPP" >> /etc/hosts

#######################################
# Update /etc/issue and /etc/issue.net
echo "FPP - Updating /etc/issue*"
head -1 /etc/issue.net > /etc/issue.new
cat >> /etc/issue.new <<EOF

Falcon Player OS Image v${FPPIMAGEVER}

EOF
cp /etc/issue.new /etc/issue
cp /etc/issue.new /etc/issue.net
rm /etc/issue.new

#######################################
# Setup for U.S. users mainly
echo "FPP - Setting US keyboard layout and locale"
sed -i "s/XKBLAYOUT=".*"/XKBLAYOUT="us"/" /etc/default/keyboard
echo "LANG=en_US.UTF-8" > /etc/default/locale

#######################################
# Make sure /opt exists
echo "FPP - Checking for existence of /opt"
cd /opt 2> /dev/null || mkdir /opt

#######################################
# Remove old /opt/fpp if it exists
if [ -e "/opt/fpp" ]
then
	echo "FPP - Removing old /opt/fpp"
	rm -rf /opt/fpp
fi


#######################################
# Make sure dependencies are installed
# Set noninteractive install to skip prompt about restarting services
export DEBIAN_FRONTEND=noninteractive



#######################################
# Clone git repository
echo "FPP - Cloning git repository into /opt/fpp"
cd /opt
git clone https://github.com/FalconChristmas/fpp fpp

#######################################
# Switch to desired code branch
echo "FPP - Switching git clone to ${FPPBRANCH} branch"
cd /opt/fpp
git checkout ${FPPBRANCH}

#######################################
# Upgrade the config if needed
sh scripts/upgrade_config

#######################################
echo "FPP - Installing PHP composer"
cd /tmp/
curl -sS https://getcomposer.org/installer | php
mv ./composer.phar /usr/local/bin/composer
chmod 755 /usr/local/bin/composer

#######################################
PHPDIR="/etc/php/7.0"
if [ -d "/etc/php/7.3" ]; then
    PHPDIR="/etc/php/7.3"
fi

echo "FPP - Allowing short tags in PHP"
FILES="cli/php.ini apache2/php.ini fpm/php.ini"
for FILE in ${FILES}
do
    sed -i -e "s/^short_open_tag.*/short_open_tag = On/" ${PHPDIR}/${FILE}
    sed -i -e "s/max_execution_time.*/max_execution_time = 1000/" ${PHPDIR}/${FILE}
    sed -i -e "s/max_input_time.*/max_input_time = 900/" ${PHPDIR}/${FILE}
    sed -i -e "s/default_socket_timeout.*/default_socket_timeout = 900/" ${PHPDIR}/${FILE}
    sed -i -e "s/post_max_size.*/post_max_size = 4G/" ${PHPDIR}/${FILE}
    sed -i -e "s/upload_max_filesize.*/upload_max_filesize = 4G/" ${PHPDIR}/${FILE}
    sed -i -e "s/;upload_tmp_dir =.*/upload_tmp_dir = \/home\/${FPPUSER}\/media\/upload/" ${PHPDIR}/${FILE}
    sed -i -e "s/^; max_input_vars.*/max_input_vars = 5000/" ${PHPDIR}/${FILE}
done

#######################################
echo "FPP - Copying rsync daemon config files into place"
sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" -e "s#FPPUSER#${FPPUSER}#g" < ${FPPDIR}/etc/rsync > /etc/default/rsync
sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" -e "s#FPPUSER#${FPPUSER}#g" < ${FPPDIR}/etc/rsyncd.conf > /etc/rsyncd.conf

#######################################
# Add the ${FPPUSER} user and group memberships
echo "FPP - Adding ${FPPUSER} user"
addgroup --gid 500 ${FPPUSER}
adduser --uid 500 --home ${FPPHOME} --shell /bin/bash --ingroup ${FPPUSER} --gecos "Falcon Player" --disabled-password ${FPPUSER}
adduser ${FPPUSER} adm
adduser ${FPPUSER} sudo
case "${FPPPLATFORM}" in
	'Raspberry Pi'|'BeagleBone Black')
		adduser ${FPPUSER} spi
        adduser ${FPPUSER} gpio
		;;
esac
adduser ${FPPUSER} video
# FIXME, use ${FPPUSER} here instead of hardcoding
sed -i -e 's/^fpp:\*:/fpp:\$6\$rA953Jvd\$oOoLypAK8pAnRYgQQhcwl0jQs8y0zdx1Mh77f7EgKPFNk\/jGPlOiNQOtE.ZQXTK79Gfg.8e3VwtcCuwz2BOTR.:/' /etc/shadow

echo "FPP - Disabling any stock 'debian' user, use the '${FPPUSER}' user instead"
sed -i -e "s/^debian:.*/debian:*:16372:0:99999:7:::/" /etc/shadow

#######################################
echo "FPP - Fixing empty root passwd"
sed -i -e 's/root::/root:*:/' /etc/shadow

#######################################
echo "FPP - Populating ${FPPHOME}"
mkdir ${FPPHOME}/.ssh
chown ${FPPUSER}.${FPPUSER} ${FPPHOME}/.ssh
chmod 700 ${FPPHOME}/.ssh

mkdir ${FPPHOME}/media
chown ${FPPUSER}.${FPPUSER} ${FPPHOME}/media
chmod 770 ${FPPHOME}/media
chmod a+s ${FPPHOME}/media
touch ${FPPHOME}/media/.auto_update_disabled
chmod 644 ${FPPHOME}/media/.auto_update_disabled

cat > ${FPPHOME}/.vimrc <<-EOF
set tabstop=4
set shiftwidth=4
set autoindent
set ignorecase
set mouse=r
EOF

chmod 644 ${FPPHOME}/.vimrc
chown ${FPPUSER}.${FPPUSER} ${FPPHOME}/.vimrc

echo >> ${FPPHOME}/.bashrc
echo ". /opt/fpp/scripts/common" >> ${FPPHOME}/.bashrc
echo >> ${FPPHOME}/.bashrc

mkdir ${FPPHOME}/media/logs
chown fpp.fpp ${FPPHOME}/media/logs

#######################################
# Configure log rotation
echo "FPP - Configuring log rotation"
cp /opt/fpp/etc/logrotate.d/* /etc/logrotate.d/
sed -i -e "s/#compress/compress/" /etc/logrotate.conf
sed -i -e "s/rotate .*/rotate 2/" /etc/logrotate.conf

#######################################
# Configure ccache
echo "FPP - Configuring ccache"
ccache -M 50M

#######################################
echo "FPP - Configuring FTP server"
sed -i -e "s/.*anonymous_enable.*/anonymous_enable=NO/" /etc/vsftpd.conf
sed -i -e "s/.*local_enable.*/local_enable=YES/" /etc/vsftpd.conf
sed -i -e "s/.*write_enable.*/write_enable=YES/" /etc/vsftpd.conf
service vsftpd restart

#######################################
echo "FPP - Configuring Samba"
cat <<-EOF >> /etc/samba/smb.conf

[FPP]
  comment = FPP Media Share
  path = ${FPPHOME}/media
  writeable = Yes
  only guest = Yes
  create mask = 0777
  directory mask = 0777
  browseable = Yes
  public = yes
  force user = ${FPPUSER}

EOF
case "${OSVER}" in
    debian_9)
        systemctl restart smbd.service
        systemctl restart nmbd.service
        ;;
esac

#######################################
# Setup mail
echo "FPP - Updating exim4 config file"
cat <<-EOF > /etc/exim4/update-exim4.conf.conf
# /etc/exim4/update-exim4.conf.conf
#
# Edit this file and /etc/mailname by hand and execute update-exim4.conf
# yourself or use 'dpkg-reconfigure exim4-config'
#
# Please note that this is _not_ a dpkg-conffile and that automatic changes
# to this file might happen. The code handling this will honor your local
# changes, so this is usually fine, but will break local schemes that mess
# around with multiple versions of the file.
#
# update-exim4.conf uses this file to determine variable values to generate
# exim configuration macros for the configuration file.
#
# Most settings found in here do have corresponding questions in the
# Debconf configuration, but not all of them.
#
# This is a Debian specific file
dc_eximconfig_configtype='smarthost'
dc_other_hostnames='fpp'
dc_local_interfaces='127.0.0.1'
dc_readhost=''
dc_relay_domains=''
dc_minimaldns='false'
dc_relay_nets=''
dc_smarthost='smtp.gmail.com::587'
CFILEMODE='644'
dc_use_split_config='true'
dc_hide_mailname='false'
dc_mailname_in_oh='true'
dc_localdelivery='mail_spool'
EOF

# remove exim4 panic log so exim4 doesn't throw an alert about a non-zero log
# file due to some odd error thrown during inital setup
rm /var/log/exim4/paniclog
#update config and restart exim
update-exim4.conf

#######################################
# Fix sudoers to not require password
echo "FPP - Giving ${FPPUSER} user sudo"
echo "${FPPUSER} ALL=(ALL:ALL) NOPASSWD: ALL" >> /etc/sudoers

#######################################
# Print notice during login regarding console access
cat <<-EOF >> /etc/motd
[0;31m
                   _______  ___
                  / __/ _ \\/ _ \\
                 / _// ___/ ___/ [0m Falcon Player[0;31m
                /_/ /_/  /_/
[1m
This FPP console is for advanced users, debugging, and developers.  If
you aren't one of those, you're probably looking for the web-based GUI.

You can access the UI by typing "http://fpp.local/" into a web browser.[0m
	EOF

#######################################
# Config fstab to mount some filesystems as tmpfs
echo "FPP - Configuring tmpfs filesystems"
echo "#####################################" >> /etc/fstab
echo "tmpfs         /tmp        tmpfs   nodev,nosuid,size=50M 0 0" >> /etc/fstab
echo "tmpfs         /var/tmp    tmpfs   nodev,nosuid,size=50M 0 0" >> /etc/fstab
echo "#####################################" >> /etc/fstab

COMMENTED=""
SDA1=$(lsblk -l | grep sda1 | awk '{print $7}')
if [ -n ${SDA1} ]
then
	COMMENTED="#"
fi
echo "${COMMENTED}/dev/sda1     ${FPPHOME}/media  auto    defaults,nonempty,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500  0  0" >> /etc/fstab
echo "#####################################" >> /etc/fstab




#######################################
echo "FPP - Configuring Apache webserver"

# Environment variables
sed -i -e "s/APACHE_RUN_USER=.*/APACHE_RUN_USER=${FPPUSER}/" /etc/apache2/envvars
sed -i -e "s/APACHE_RUN_GROUP=.*/APACHE_RUN_GROUP=${FPPUSER}/" /etc/apache2/envvars
sed -i -e "s#APACHE_LOG_DIR=.*#APACHE_LOG_DIR=${FPPHOME}/media/logs#" /etc/apache2/envvars
sed -i -e "s/Listen 8080.*/Listen 80/" /etc/apache2/ports.conf

sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" < ${FPPDIR}/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf

# Enable Apache modules
a2enmod cgi
a2enmod rewrite
a2enmod proxy
a2enmod proxy_http

# Fix name of Apache default error log so it gets rotated by our logrotate config
sed -i -e "s/error\.log/apache2-base-error.log/" /etc/apache2/apache2.conf

# Disable private /tmp directory
sed -i -e "s/PrivateTmp=true/PrivateTmp=false/" /lib/systemd/system/apache2.service

# Disable default access logs
rm /etc/apache2/conf-enabled/other-vhosts-access-log.conf

#######################################
echo "FPP - Updating HotSpot"
sed -i -e "s/USE_PERSONAL_SSID=.*/USE_PERSONAL_SSID=FPP/" /etc/default/bb-wl18xx
sed -i -e "s/USE_PERSONAL_PASSWORD=.*/USE_PERSONAL_PASSWORD=Christmas/" /etc/default/bb-wl18xx

#######################################
echo "FPP - Configuring FPP startup"
cp /opt/fpp/etc/systemd/*.service /lib/systemd/system/
systemctl enable fppinit.service
systemctl enable fppcapedetect.service
systemctl enable fpprtc.service
systemctl enable fppoled.service
systemctl enable fppd.service

systemctl enable rsync

echo "FPP - Disabling services not needed/used"
systemctl disable olad
systemctl disable dev-hugepages.mount

echo "FPP - update BBB boot scripts"
cd /opt/scripts
git reset --hard
git pull
sed -i 's/systemctl restart serial-getty/systemctl is-enabled serial-getty/g' boot/am335x_evm.sh

echo "FPP - Compiling binaries"
cd /opt/fpp/src/
make clean ; make optimized

ENDTIME=$(date)

echo "========================================================="
echo "FPP Install Complete."
echo "Started : ${STARTTIME}"
echo "Finished: ${ENDTIME}"
echo "========================================================="
echo "You can reboot the system by changing to the '${FPPUSER} user with the"
echo "password 'falcon' and running the shutdown command."
echo ""
echo "su - ${FPPUSER}
echo "sudo shutdown -r now"
echo ""
echo "NOTE: If you are prepping this as an image for release,"
echo "remove the SSH keys before shutting down so they will be"
echo "rebuilt during the next boot."
echo ""
echo "su - ${FPPUSER}
echo "sudo rm -rf /etc/ssh/ssh_host*key*"
echo "sudo shutdown -r now"
echo "========================================================="
echo ""
