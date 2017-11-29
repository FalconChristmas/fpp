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
# sudo ./FPP_Install.sh
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
#           - URL: FIXME
#           - Image
#             - 
#           - Login/Password
#             - pi/raspberry
#
#       BeagleBone Black
#           - URL: https://rcn-ee.com/rootfs/bb.org/release/2014-05-14/
#           - Images
#             - bone-debian-7.5-2014-05-14-2gb.img
#             - BBB-eMMC-flasher-debian-7.5-2014-05-14-2gb.img
#           - Login
#             - root (no password)
#
#       Other OS images may work with this install script and FPP on the
#       Pi and BBB platforms, but these are the images we are currently
#       targetting for support.
#
#############################################################################
# Other platforms which may be functioning with varying degrees:
#
#       ODROID C1
#           http://recombi.net/odroid-c1_deb-7.8/
#           - odroid-c1-debian-7.8-03-29-2014.img
#           - Login/Password
#             - root/odroid
#           - This image is very small and a lot of packages get installed
#             by FPP_Install.sh, so make sure you resize/expand the root
#             filesystem as per the information.txt file on recombi.net.
#
#############################################################################
SCRIPTVER="0.8"
FPPBRANCH="v1.6"
FPPIMAGEVER="1.5"
FPPCFGVER="17"
FPPPLATFORM="UNKNOWN"
FPPDIR="/opt/fpp"
OSVER="UNKNOWN"

# FIXME, need to handle config version 14 to fix force HDMI on the Pi
# can we do this at install time or does it need to be at first boot?
# do we need a FPP "first boot" script for this and other things?

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
echo "  existing 'pi' or 'debian' user and create a 'fpp' user.  If the system"
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

case "${OSVER}" in
	debian_7)
		echo "FPP - Enabling non-free repo"
		sed -i -e "s/^deb \(.*\)/deb \1 non-free/" /etc/apt/sources.list
		sed -i -e "s/non-free\(.*\)non-free/non-free\1/" /etc/apt/sources.list

		echo "FPP - Updating package list"
		apt-get update

		echo "FPP - Removing some unneeded packages"
		apt-get -y remove gnome-icon-theme gnome-accessibility-themes gnome-themes-standard gnome-themes-standard-data libsoup-gnome2.4-1:armhf desktop-base xserver-xorg x11proto-composite-dev x11proto-core-dev x11proto-damage-dev x11proto-fixes-dev x11proto-input-dev x11proto-kb-dev x11proto-randr-dev x11proto-render-dev x11proto-xext-dev x11proto-xinerama-dev xchat xrdp xscreensaver xscreensaver-data desktop-file-utils dbus-x11 javascript-common ruby1.9.1 ruby libxxf86vm1:armhf libxxf86dga1:armhf libxvidcore4:armhf libxv1:armhf libxtst6:armhf libxslt1.1:armhf libxres1:armhf libxrender1:armhf  libxrandr2:armhf libxml2-dev libxmuu1 xauth wvdial xserver-xorg-video-fbdev xfonts-utils xfonts-encodings   libuniconf4.6 libwvstreams4.6-base libwvstreams4.6-extras 
		# Should be able to add these as well:
		# poppler-data desktop-base libsane libsane-extras sane-utils freepats xserver-xorg-video-modesetting xserver-xorg-core xserver-xorg xserver-common x11-xserver-utils xscreensaver xrdp
		apt-get -y --purge autoremove
		# remove gnome keyring module config which causes pkcs11 warnings
		# when trying to do a git pull
		rm /etc/pkcs11/modules/gnome-keyring-module

		echo "FPP - Installing required packages (Set #1)"
		# Install in more than one command to lower total disk space required
		# Do a clean in between each iteration
		apt-get -y install alsa-base alsa-utils apache2 apache2.2-bin apache2.2-common apache2-mpm-prefork apache2-utils arping avahi-daemon avahi-discover avahi-utils bash-completion bc build-essential bzip2 ca-certificates ccache curl device-tree-compiler dh-autoreconf
		apt-get -y clean

		# FIXME, newer debian has newer gcc, don't bother installing old version
		# if newer version is already installed.
		echo "FPP - Installing required packages (Set #2)"
		apt-get -y install 'g++-4.7' gcc-4.7
		apt-get -y clean

		echo "FPP - Installing required packages (Set #3)"
		apt-get -y install ethtool exfat-fuse fbi fbset file flite gdb gdebi-core git i2c-tools ifplugd imagemagick less libapache2-mod-php5 libboost-dev libconvert-binary-c-perl libdbus-glib-1-dev libdevice-serialport-perl libjs-jquery libjs-jquery-ui libjson-perl libjsoncpp-dev
		apt-get -y clean

		echo "FPP - Installing required packages (Set #4)"
		apt-get -y install libnet-bonjour-perl libpam-smbpass libtagc0-dev libtest-nowarnings-perl locales mp3info mpg123 mpg321 mplayer nano nginx node ntp perlmagick php5 php5-cli php5-common php5-curl php5-fpm php5-mcrypt php5-sqlite php-apc python-daemon python-smbus samba samba-common-bin shellinabox sudo sysstat tcpdump usbmount vim vim-common vorbis-tools vsftpd
		apt-get -y clean

		echo "FPP - Installing wireless firmware packages"
		apt-get -y install firmware-realtek

		echo "FPP - Cleaning up after installing packages"
		apt-get -y clean

		echo "FPP - Make things cleaner by removing configuration from unnecessary packages"
		dpkg --get-selections | grep deinstall | while read package deinstall; do
			apt-get -y purge $package
		done

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

			# Disabling nginx & php-fpm for now until new UI is in place
			systemctl disable nginx.service
			systemctl disable php5-fpm.service
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
			echo "FPP - WARNING: This list may be incomplete, it needs to be updated to match the Pi/BBB Debian package install list"
			apt-get -y install apache2 apache2-bin apache2-mpm-prefork apache2-utils avahi-discover fbi flite gdebi-core i2c-tools imagemagick libapache2-mod-php5 libboost-dev libconvert-binary-c-perl libjson-perl libjsoncpp-dev libnet-bonjour-perl libpam-smbpass mp3info mpg123 perlmagick php5 php5-cli php5-common php-apc python-daemon python-smbus samba samba-common-bin shellinabox sysstat vorbis-tools vsftpd
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
		echo >> /boot/uboot/uEnv.txt

		echo "FPP - Installing OLA packages"
		apt-get -y --force-yes install libcppunit-dev uuid-dev pkg-config libncurses5-dev libtool autoconf automake libmicrohttpd-dev protobuf-compiler python-protobuf libprotobuf-dev libprotoc-dev zlib1g-dev bison flex libftdi-dev libftdi1 libusb-1.0-0-dev liblo-dev
		apt-get -y clean

		mkdir /tmp/deb
		cd /tmp/deb
		FILES="libola-dev_0.9.7-1_armhf.deb libola1_0.9.7-1_armhf.deb ola-python_0.9.7-1_all.deb ola-rdm-tests_0.9.7-1_all.deb ola_0.9.7-1_armhf.deb"
		for FILE in ${FILES}
		do
			# FIXME, get these from github after release is tagged
			wget -nd http://www.bc2va.org/chris/tmp/fpp/deb/${FILE}
		done
		dpkg --unpack ${FILES}
		rm -f ${FILES}
#		apt-get -y --force-yes install libcppunit-dev libcppunit-1.12-1 uuid-dev pkg-config libncurses5-dev libtool autoconf automake  libmicrohttpd-dev protobuf-compiler python-protobuf libprotobuf-dev libprotoc-dev zlib1g-dev bison flex libftdi-dev libftdi1 libusb-1.0-0-dev liblo-dev
#		git clone https://github.com/OpenLightingProject/ola.git /opt/ola
#		(cd /opt/ola && autoreconf -i && ./configure --enable-python-libs && make && make install && ldconfig && cd /opt/ && rm -rf ola)

		echo "FPP - Installing updated 8192cu module"
		wget -O /lib/modules/3.8.13-bone50/kernel/drivers/net/wireless/8192cu.ko https://github.com/FalconChristmas/fpp/releases/download/1.5/8192cu.ko

		echo "FPP - Disabling power management for 8192cu wireless"
cat <<-EOF >> /etc/modprobe.d/8192cu.conf
		# Blacklist native RealTek 8188CUs drivers
		blacklist rtl8192cu
		blacklist rtl8192c_common
		blacklist rtlwifi

		# Disable power management on 8192cu module
		options 8192cu rtw_power_mgnt=0 rtw_enusbss=0
EOF

		;;

	'Raspberry Pi')
		wget http://goo.gl/1BOfJ -O /usr/bin/rpi-update && chmod +x /usr/bin/rpi-update
		SKIP_WARNING=1 rpi-update d4945b3b77d29cc5bb3777734422c048c1f1d003

		echo "FPP - Installing Pi-specific packages"
		apt-get -y install raspi-config

		echo "FPP - Installing OLA packages"
		echo "deb http://apt.openlighting.org/raspbian wheezy main" > /etc/apt/sources.list.d/ola.list
		apt-get update
		apt-get -y --force-yes install ola ola-rdm-tests ola-conf-plugins ola-dev libprotobuf-dev

		echo "FPP - Updating packages"
		apt-get -y upgrade

		echo "FPP - Installing wiringPi"
		cd /opt/ && git clone git://git.drogon.net/wiringPi && cd /opt/wiringPi && ./build

		if [ "x$1" == "x--build-omxplayer" ]; then
			echo "FPP - Building omxplayer from source with our patch"
			apt-get -y install subversion libpcre3-dev libboost-dev libfreetype6-dev libusb-1.0-0-dev
			apt-get -y install git-core libidn11-dev libssl1.0-dev libssh-dev libsmbclient-dev libasound2-dev
			git clone https://github.com/popcornmix/omxplayer.git
			cd omxplayer
			# get the latest and greatest to support ALSA
			#git reset --hard 4d8ffd13153bfef2966671cb4fb484afeaf792a8
			wget -O- https://raw.githubusercontent.com/FalconChristmas/fpp/master-v1.x/external/omxplayer/FPP_omxplayer.diff | patch -p1
			./prepare-native-raspbian.sh
			sed -i -e "s/PWD/shell pwd/" Makefile.ffmpeg
			make ffmpeg
			make
			tar xzpvf omxplayer-dist.tgz -C /
			cd ..
		else
			echo "FPP - Installing patched omxplayer.bin for FPP MultiSync"
			apt-get -y install libssh-4
			wget -O- https://github.com/FalconChristmas/fpp-binaries/raw/master/Pi/omxplayer-dist.tgz | tar xzpv -C /
		fi

		echo "FPP - Disabling getty on onboard serial ttyAMA0"
		sed -i "s@T0:23:respawn:/sbin/getty -L ttyAMA0@#T0:23:respawn:/sbin/getty -L ttyAMA0@" /etc/inittab

		echo "FPP - Disabling the hdmi force hotplug setting"
		sed -i -e "s/hdmi_force_hotplug/#hdmi_force_hotplug/" /boot/config.txt

		echo "FPP - Enabling SPI in device tree"
		echo >> /boot/config.txt

		echo "# Enable SPI in device tree" >> /boot/config.txt
		echo "dtparam=spi=on" >> /boot/config.txt
		echo >> /boot/config.txt

		echo "FPP - Updating SPI buffer size"
		sed -i 's/$/ spidev.bufsiz=102400/' /boot/cmdline.txt

		echo "# Enable I2C in device tree" >> /boot/config.txt
		echo "dtparam=i2c=on" >> /boot/config.txt
		echo >> /boot/config.txt

		echo "# Setting kernel scaling framebuffer method" >> /boot/config.txt
		echo "scaling_kernel=8" >> /boot/config.txt
		echo >> /boot/config.txt

		echo "# Allow more current through USB" >> /boot/config.txt
		echo "max_usb_current=1" >> /boot/config.txt
		echo >> /boot/config.txt

		echo "FPP - Freeing up more space by removing unnecessary packages"
		apt-get -y purge wolfram-engine sonic-pi minecraft-pi
		apt-get -y --purge autoremove

		echo "FPP - Make things cleaner by removing configuration from unnecessary packages"
		dpkg --get-selections | grep deinstall | while read package deinstall; do
			apt-get -y purge $package
		done

		echo "FPP - Disabling power management for wireless"
		echo -e "# Disable power management\noptions 8192cu rtw_power_mgnt=0 rtw_enusbss=0" > /etc/modprobe.d/8192cu.conf

		echo "FPP - Disabling Swap to save SD card"
		update-rc.d -f dphys-swapfile remove

		echo "FPP - Kernel doesn't support cgroups so remove to silence warnings on boot"
		update-rc.d -f cgroup-bin remove

		echo "FPP - Remove dhcpcd since we start DHCP interfaces on our own"
		update-rc.d -f dhcpcd remove

		echo "FPP - Setting locale"
		sed -i 's/^\(en_GB.UTF-8\)/# \1/;s/..\(en_US.UTF-8\)/\1/' /etc/locale.gen
		locale-gen en_US.UTF-8
		dpkg-reconfigure --frontend=noninteractive locales
		export LANG=en_US.UTF-8

		echo "FPP - Fix ifup/ifdown scripts for manual dns"
		sed -i -n 'H;${x;s/^\n//;s/esac\n/&\nif grep -qc "Generated by fpp" \/etc\/resolv.conf\; then\n\texit 0\nfi\n/;p}' /etc/network/if-up.d/000resolvconf
		sed -i -n 'H;${x;s/^\n//;s/esac\n/&\nif grep -qc "Generated by fpp" \/etc\/resolv.conf\; then\n\texit 0\nfi\n\n/;p}' /etc/network/if-down.d/resolvconf

		echo "FPP - Disable nginx to avoid warning"
		# Note, when we switch to the v2.0 UI we'll need to update this script to remove apache instead
		update-rc.d -f nginx remove
		;;

	'ODROID')
		echo "FPP - Installing wiringPi"
		cd /opt/ && git clone https://github.com/hardkernel/wiringPi && cd /opt/wiringPi && ./build

		echo "FPP - Installing patched omxplayer.bin for FPP MultiSync"
		cp /usr/bin/omxplayer.bin /usr/bin/omxplayer.bin.orig
		wget -O /usr/bin/omxplayer.bin https://github.com/FalconChristmas/fpp-binaries/raw/master/Pi/omxplayer.bin
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
echo "FPP - Installing PHP composer"
cd /tmp/
curl -sS https://getcomposer.org/installer | php
mv ./composer.phar /usr/local/bin/composer
chmod 755 /usr/local/bin/composer

#######################################
echo "FPP - Setting up for UI"
sed -i -e "s/^user =.*/user = fpp/" /etc/php5/fpm/pool.d/www.conf
sed -i -e "s/^group =.*/group = fpp/" /etc/php5/fpm/pool.d/www.conf

#######################################
# echo "FPP - Composing FPP UI"
# FIXME, eventually run composer here in the new UI dir

#######################################
# Add the fpp user and group memberships
echo "FPP - Adding fpp user"
addgroup --gid 500 fpp
adduser --uid 500 --home /home/fpp --shell /bin/bash --ingroup fpp --gecos "Falcon Player" --disabled-password fpp
adduser fpp adm
adduser fpp sudo
adduser fpp spi
adduser fpp video
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

echo >> /home/fpp/.bashrc
echo ". /opt/fpp/scripts/common" >> /home/fpp/.bashrc
echo >> /home/fpp/.bashrc

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

COMMENTED=""
SDA1=$(lsblk -l | grep sda1 | awk '{print $7}')
if [ -n ${SDA1} ]
then
	COMMENTED="#"
fi
echo "${COMMENTED}/dev/sda1     /home/fpp/media  auto    defaults,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500  0  0" >> /etc/fstab
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
ISAPACHE24=$(dpkg -l | grep "^ii *apache2 *2\.4\.")
if [ -z ${ISAPACHE24} ]
then
	cp ${FPPDIR}/etc/apache2.conf /etc/apache2/apache2.conf
else
	cp ${FPPDIR}/etc/apache2.conf.2.4 /etc/apache2/apache2.conf
	rm /etc/apache2/sites-enabled/000-default.conf
fi

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

ENDTIME=$(date)

echo "========================================================="
echo "FPP Install Complete."
echo "Started : ${STARTTIME}"
echo "Finished: ${ENDTIME}"
echo "========================================================="
echo "You can reboot the system by changing to the 'fpp' user with the"
echo "password 'falcon' and running the shutdown command."
echo ""
echo "su - fpp"
echo "sudo shutdown -r now"
echo ""
echo "NOTE: If you are prepping this as an image for release,"
echo "remove the SSH keys before shutting down so they will be"
echo "rebuilt during the next boot."
echo ""
echo "su - fpp"
echo "sudo rm -rf /etc/ssh/ssh_host*key*"
echo "sudo shutdown -r now"
echo "========================================================="
echo ""

