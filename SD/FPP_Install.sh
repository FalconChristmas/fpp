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
# wget -O ./FPP_Install.sh https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/FPP_Install.sh
# chmod 700 ./FPP_Install.sh
# sudo ./FPP_Install.sh
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

		echo "FPP - Uninstalling unneeded packages to save space"
		apt-get -y remove xchat xrdp 'xscreensaver*' wvdial tightvncserver sane-utils ppp openbox openbox-themes lxde-common lxde-core lxmenu-data lxpanel lxsession lxterminal 'lightdm*' 

		echo "FPP - Installing required packages"
		apt-get -y install alsa-base alsa-utils apache2 apache2.2-bin apache2.2-common apache2-mpm-prefork apache2-utils arping avahi-daemon avahi-discover avahi-utils bc build-essential bzip2 ca-certificates ccache curl device-tree-compiler ethtool fbi file flite 'g++-4.7' gcc-4.7 gdb git i2c-tools ifplugd imagemagick less libapache2-mod-php5 libconvert-binary-c-perl libdbus-glib-1-dev libdevice-serialport-perl libjson-perl libnet-bonjour-perl libpam-smbpass libtagc0-dev locales mp3info mpg123 mplayer node perlmagick php5 php5-cli php5-common php-apc python-daemon python-smbus samba samba-common-bin shellinabox sudo sysstat usbmount vim vim-common vorbis-tools vsftpd

		echo "FPP - Installing non-packaged Perl modules via CPAN"
		echo "yes" | cpan -fi File::Map Net::WebSocket::Server

		# gcc/g++ v4.6 are pulled in as a dependency for something above so
		# switch the system to use the 4.7 version
		if [ -h /usr/bin/gcc -a -f /usr/bin/gcc-4.7 ]
		then
			rm /usr/bin/gcc
			ln -s /usr/bin/gcc-4.7 /usr/bin/gcc
		fi

		if [ -h /usr/bin/g++ -a -f /usr/bin/g++-4.7 ]
		then
			rm /usr/bin/g++
			ln -s /usr/bin/g++-4.7 /usr/bin/g++
		fi

		echo "FPP - Disabling any stock 'debian' user, use the 'fpp' user instead"
		sed -i -e "s/^debian:.*/debian:*:16372:0:99999:7:::/" /etc/shadow

		echo "FPP - Disabling any stock 'pi' user, use the 'fpp' user instead"
		sed -i -e "s/^pi:.*/pi:*:16372:0:99999:7:::/" /etc/shadow

		if [ -f /bin/systemctl ]
		then
			echo "FPP - Disabling unneeded/unwanted services"
			systemctl disable bonescript.socket
			systemctl disable bonescript-autorun.service
			systemctl disable console-kit-daemon.service
			systemctl disable cloud9.socket
		fi

		echo "FPP - Disabling GUI"
		update-rc.d -f gdm remove
		update-rc.d -f gdm3 remove
		update-rc.d -f lightdm remove
		update-rc.d -f wdm remove
		update-rc.d -f xdm remove

		;;
	ubuntu_14.04)
		echo "FPP - Updating package list"
		apt-get update
		echo "FPP - Installing required packages"
		if [ "x${FPPPLATFORM}" = "xODROID" ]
		then
			apt-get -y install apache2 apache2-bin apache2-mpm-prefork apache2-utils avahi-discover fbi flite i2c-tools imagemagick libapache2-mod-php5 libconvert-binary-c-perl libjson-perl libnet-bonjour-perl libpam-smbpass mp3info mpg123 perlmagick php5 php5-cli php5-common php-apc python-daemon python-smbus samba samba-common-bin shellinabox sysstat vorbis-tools vsftpd
		fi
		;;
esac

#######################################
# Platform-specific config
case "${FPPPLATFORM}" in
	'BeagleBone Black')
		echo "FPP - Disabling HDMI for Falcon and LEDscape cape support"
		echo >> /boot/uboot/uEnv.txt
		echo "# Disable HDMI for Falcon and LEDscape cape support" >> /boot/uboot/uEnv.txt
		echo "cape_disable=capemgr.disable_partno=BB-BONELT-HDMI,BB-BONELT-HDMIN" >> /boot/uboot/uEnv.txt
		;;

	'Raspberry Pi')
		echo "FPP - Installing wiringPi"
		cd /opt/ && git clone git://git.drogon.net/wiringPi && cd /opt/wiringPi && ./build

		echo "FPP - Installing patched omxplayer.bin for FPP MultiSync"
		cp /usr/bin/omxplayer.bin /usr/bin/omxplayer.bin.orig
		wget -O /usr/bin/omxplayer.bin https://raw.githubusercontent.com/FalconChristmas/fpp-binaries/Pi/omxplayer.bin
		;;

	'ODROID')
		echo "FPP - Installing wiringPi"
		cd /opt/ && git clone https://github.com/hardkernel/wiringPi && cd /opt/wiringPi && ./build

		echo "FPP - Installing patched omxplayer.bin for FPP MultiSync"
		cp /usr/bin/omxplayer.bin /usr/bin/omxplayer.bin.orig
		wget -O /usr/bin/omxplayer.bin https://raw.githubusercontent.com/FalconChristmas/fpp-binaries/Pi/omxplayer.bin
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
addgroup --gid 500 fpp
adduser --uid 500 --home /home/fpp --shell /bin/bash --ingroup fpp --gecos "Falcon Player" --disabled-password fpp
adduser fpp adm
adduser fpp sudo
sed -i -e 's/^fpp:\*:/fpp:\$6\$rA953Jvd\$oOoLypAK8pAnRYgQQhcwl0jQs8y0zdx1Mh77f7EgKPFNk\/jGPlOiNQOtE.ZQXTK79Gfg.8e3VwtcCuwz2BOTR.:/' /etc/shadow

#######################################
echo "FPP - Fixing empty root passwd"
sed -i -e 's/root::/root:*:/' /etc/shadow

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
  comment = FPP Home Share
  path = /home/fpp
  writeable = Yes
  only guest = Yes
  create mask = 0777
  directory mask = 0777
  browseable = Yes
  public = yes
  force user = fpp

EOF
service samba restart


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
echo "/dev/sda1     /home/fpp/media  auto    defaults,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500  0  0" >> /etc/fstab
echo "#####################################" >> /etc/fstab


#######################################
# Disable IPv6
echo "FPP - Disabling IPv6"
cat <<-EOF >> /etc/sysctl.conf

	# FPP - Disable IPv6
	net.ipv6.conf.all.disable_ipv6 = 1
	net.ipv6.conf.default.disable_ipv6 = 1
	net.ipv6.conf.lo.disable_ipv6 = 1
	net.ipv6.conf.eth0.disable_ipv6 = 1
	EOF

#######################################
# Configure Apache run user/group
echo "FPP - Configuring Apache"
# environment variables
sed -i -e "s/APACHE_RUN_USER=.*/APACHE_RUN_USER=fpp/" /etc/apache2/envvars
sed -i -e "s/APACHE_RUN_GROUP=.*/APACHE_RUN_GROUP=fpp/" /etc/apache2/envvars

# main Apache/PHP config
cp ${FPPDIR}/etc/apache2.conf /etc/apache2/
cp ${FPPDIR}/etc/php.ini /etc/php5/apache2/php.ini

# site config file
SITEFILE="/etc/apache2/sites-enabled/000-default"
sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#/home/pi/#/home/fpp/#g" < ${FPPDIR}/etc/apache2.site > ${SITEFILE}
# Disable Logging since there is no logs directory yet
sed -i "s/ErrorLog/#ErrorLog/" ${SITEFILE}

case "${OSVER}" in
	debian_7)
				rm /etc/apache2/conf.d/other-vhosts-access-log
				sed -i -e "s/NameVirtualHost.*8080/NameVirtualHost *:80/" -e "s/Listen.*8080/Listen 80/" /etc/apache2/ports.conf
				;;
	ubuntu_14.04)
				sed -i -e "s/^Include conf.d/#Include conf.d/" /etc/apache2/apache2.conf
				sed -i -e "s/^LockFile/#LockFile/" /etc/apache2/apache2.conf
				rm /etc/apache2/sites-enabled/000-default.conf
				;;
esac

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

echo "FPP - Compiling binaries"
cd /opt/fpp/src/
make clean ; make

echo "========================================================="
echo "FPP Install Complete, you can reboot the system by su-ing"
echo "to the 'fpp' user (password 'falcon') and running the"
echo "shutdown command."
echo ""
echo "su - fpp"
echo "sudo shutdown -r now"
echo "========================================================="
echo ""

#######################################
# FPP_Install.sh TODO List
# Bugs
# - some files owned by root under /home/fpp somehow
#   - bytesReceived, schedule, settings, universes, config/Falcon.FPDV1
#   - appears to be when media is not FAT mounted as pi/pi  fpp/fpp
# Raspberry Pi (officially supported FPP v2.0 platform)
# - 
# BeagleBone Black (officially supported FPP v2.0 platform)
# - Hide USB network IP in UI
# - http://elinux.org/Beagleboard:BeagleBoneBlack_Debian#2014-05-14
# - https://s3.amazonaws.com/debian.beagleboard.org/images/bone-debian-7.5-2014-05-14-2gb.img.xz
# - Setup uEnv.txt for LEDscape & copy overlay file
#   https://github.com/osresearch/LEDscape/blob/master/Setup.md
#   /boot/uboot/uEnv.txt
#     cape_disable=capemgr.disable_partno=BB-BONELT-HDMI,BB-BONELT-HDMIN
# ODROID-C1 (not officially supported in FPP v2.0)
# - Install (their patched) wiringPi
# - Handle apache config differences for Ubuntu in /opt/fpp/scripts/startup
# PogoPlug (not officially supported in FPP v2.0)
# - Initial OS install test
# - Set Platform
# - Any other Debian version differences
#######################################

