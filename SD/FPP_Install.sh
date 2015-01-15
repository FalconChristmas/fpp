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
# To use this script, download the latest copy from github and run it as
# root on the system where you want to install FPP:
#
# wget -O /tmp/FPP_Install.sh https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/FPP_Install.sh
# chmod 700 /tmp/FPP_Install.sh
# sudo /tmp/FPP_Install.sh
#
#############################################################################
SCRIPTVER="0.1"
FPPBRANCH="BBB"
FPPIMAGEVER="2.0"
FPPPLATFORM="UNKNOWN"
FPPDIR="/opt/fpp"
OSVER="UNKNOWN"
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

# Attempt to detect the platform we are installing on
ODROID=$(grep ODROIDC /proc/cpuinfo)
if [ "x${OSID}" = "xraspbian" ]
then
	FPPPLATFORM="Raspberry Pi"
	OSVER="debian_${VERSION_ID}"
elif [ -e "/sys/class/leds/beaglebone:green:usr0" ]
then
	FPPPLATFORM="BeagleBone Black"
elif [ ! -z "${ODROID}" ]
then
	FPPPLATFORM="ODROID"
else
	FPPPLATFORM="UNKNOWN"
fi

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
echo "============================================================"
#############################################################################

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
echo "6" > /etc/fpp/config_version

#######################################
# Setting hostname
echo "FPP - Setting hostname"
echo "FPP" > /etc/hostname
hostname FPP

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
case "${OSVER}" in
	debian_7)
		echo "FPP - Updating package list"
		apt-get update
		echo "FPP - Installing required packages"
		apt-get -y install alsa-base alsa-utils apache2 apache2.2-bin apache2.2-common apache2-mpm-prefork apache2-utils arping avahi-daemon avahi-discover avahi-utils bc build-essential bzip2 ca-certificates ccache curl ethtool fbi file flite 'g++-4.7' gcc-4.7 gdb git i2c-tools ifplugd imagemagick less libapache2-mod-php5 libconvert-binary-c-perl libjson-perl libnet-bonjour-perl libtagc0-dev locales mp3info mpg123 mplayer perlmagick php5 php5-cli php5-common php-apc python-daemon python-smbus sudo sysstat vim vim-common vorbis-tools
		;;
	ubuntu_14.04)
		echo "FPP - Updating package list"
		apt-get update
		echo "FPP - Installing required packages"
		if [ "x${FPPPLATFORM}" = "xODROID" ]
		then
			apt-get -y install apache2 apache2-bin apache2-mpm-prefork apache2-utils avahi-discover fbi flite i2c-tools imagemagick libapache2-mod-php5 libconvert-binary-c-perl libjson-perl libnet-bonjour-perl mp3info mpg123 perlmagick php5 php5-cli php5-common php-apc python-daemon python-smbus sysstat vorbis-tools
		fi
		;;
esac


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
# Add the fpp user and group memberships
echo "FPP - Adding fpp user"
addgroup fpp
adduser --home /home/fpp --shell /bin/bash --ingroup fpp --gecos "Falcon Player" --disabled-password fpp
adduser fpp adm
adduser fpp sudo
sed -i -e 's/^fpp:\*:/fpp:\$6\$rA953Jvd\$oOoLypAK8pAnRYgQQhcwl0jQs8y0zdx1Mh77f7EgKPFNk\/jGPlOiNQOtE.ZQXTK79Gfg.8e3VwtcCuwz2BOTR.:/' /etc/shadow

#######################################
echo "FPP - Populating /home/fpp"
mkdir /home/fpp/.ssh
chown fpp.fpp /home/fpp/.ssh
chmod 700 /home/fpp/.ssh

mkdir /home/fpp/media
chown fpp.fpp /home/fpp/media
chmod 700 /home/fpp/media

#######################################
# Add FPP hostname entry
echo "FPP - Adding 'FPP' hostname entry"
# Remove any existing 127.0.1.1 entry first
sed -i -e "/^127.0.1.1[^0-9]/d" /etc/hosts
echo "127.0.1.1       FPP" >> /etc/hosts

#######################################
# Configure log rotation
echo "FPP - Configuring log rotation"
cp /opt/fpp/etc/logrotate.d/* /etc/logrotate.d/

#######################################
# Configure ccache
echo "FPP - Configuring ccache"
ccache -M 5M

#######################################
# Fix sudoers to not require password
echo "FPP - Giving fpp user sudo"
echo "fpp ALL=(ALL:ALL) NOPASSWD: ALL" >> /etc/sudoers

#######################################
# Config fstab to mount some filesystems as tmpfs
echo "FPP - Configuring tmpfs filesystems"
echo "#####################################" >> /etc/fstab
echo "tmpfs         /var/log    tmpfs   nodev,nosuid,size=10M 0 0" >> /etc/fstab
echo "tmpfs         /var/tmp    tmpfs   nodev,nosuid,size=10M 0 0" >> /etc/fstab
echo "#####################################" >> /etc/fstab

#######################################
# Configure Apache run user/group
echo "FPP - Configuring Apache"
sed -i -e "s/APACHE_RUN_USER=.*/APACHE_RUN_USER=fpp/" /etc/apache2/envvars
sed -i -e "s/APACHE_RUN_GROUP=.*/APACHE_RUN_GROUP=fpp/" /etc/apache2/envvars
sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#/home/pi/#/home/fpp/#g" < ${FPPDIR}/etc/apache2.site > /etc/apache2/sites-available/default
# Disable Logging since there is no logs directory yet
sed -i "s/ErrorLog/#ErrorLog/" /etc/apache2/sites-available/default
cp ${FPPDIR}/etc/apache2.conf /etc/apache2/
cp ${FPPDIR}/etc/php.ini /etc/php5/apache2/php.ini

if [ "x${OSVER}" = "xubuntu_14.04" ]
then
	sed -i -e "s/^Include conf.d/#Include conf.d/" /etc/apache2/apache2.conf
	sed -i -e "s/^LockFile/#LockFile/" /etc/apache2/apache2.conf
	mv /etc/apache2/sites-available/default /etc/apache2/sites-enabled/000-default.conf
else
	rm /etc/apache2/conf.d/other-vhosts-access-log
fi

update-rc.d apache2 defaults
/etc/init.d/apache2 stop
chown fpp /var/lock/apache2
/etc/init.d/apache2 start

#######################################
echo "FPP - Configuring FPP startup"
cp /opt/fpp/etc/init.d/fppinit /etc/init.d/
update-rc.d fppinit defaults
cp /opt/fpp/etc/init.d/fppstart /etc/init.d/
update-rc.d fppstart defaults

#######################################
# FPP_Install.sh TODO List
# Raspberry Pi
# - Install wiringPi
# - Handle mounting USB flash drive by adding to /etc/fstab
# BeagleBone Black
# - Install LEDscape
# ODROID
# - Install (their patched) wiringPi
# - Handle apache config differences for Ubuntu in /opt/fpp/scripts/startup
# PogoPlug
# - Initial OS install test
# - Set Platform
#######################################

