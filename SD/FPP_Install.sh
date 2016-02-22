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
SCRIPTVER="0.9"
FPPBRANCH="master"
FPPIMAGEVER="2.0alpha"
FPPCFGVER="18"
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
	debian_7|debian_8)
		case $FPPPLATFORM in
			'CHIP'|'BeagleBone Black')
				echo "FPP - Skipping non-free for $FPPPLATFORM"
				;;
			*)
				echo "FPP - Enabling non-free repo"
				sed -i -e "s/^deb \(.*\)/deb \1 non-free/" /etc/apt/sources.list
				sed -i -e "s/non-free\(.*\)non-free/non-free\1/" /etc/apt/sources.list
				;;
		esac

		echo "FPP - Marking unneeded packages for removal to save space"
		for package in gnome-icon-theme gnome-accessibility-themes gnome-themes-standard \
						apache2 apache2-doc apache2-mpm-prefork apache2-utils \
						apache2.2-bin apache2.2-common libapache2-mod-php5 \
						gnome-themes-standard-data libsoup-gnome2.4-1:armhf desktop-base \
						xserver-xorg x11proto-composite-dev x11proto-core-dev \
						x11proto-damage-dev x11proto-fixes-dev x11proto-input-dev \
						x11proto-kb-dev x11proto-randr-dev x11proto-render-dev \
						x11proto-xext-dev x11proto-xinerama-dev xchat xrdp xscreensaver \
						xscreensaver-data desktop-file-utils dbus-x11 javascript-common \
						ruby1.9.1 ruby libxxf86vm1:armhf libxxf86dga1:armhf \
						libxvidcore4:armhf libxv1:armhf libxtst6:armhf libxslt1.1:armhf \
						libxres1:armhf libxrender1:armhf libxrandr2:armhf libxml2-dev \
						libxmuu1 xauth wvdial xserver-xorg-video-fbdev xfonts-utils \
						xfonts-encodings libuniconf4.6 libwvstreams4.6-base \
						libwvstreams4.6-extras poppler-data desktop-base libsane \
						libsane-extras sane-utils freepats xserver-xorg-video-modesetting \
						xserver-xorg-core xserver-xorg xserver-common x11-xserver-utils \
						xscreensaver xrdp bluej greenfoot oracle-java7-jdk
		do
			echo "$package deinstall" | dpkg --set-selections
		done

		echo "FPP - Make things cleaner by removing unneeded packages"
		dpkg --get-selections | grep deinstall | while read package deinstall; do
			apt-get -y purge $package
		done

		echo "FPP - Removing anything left that wasn't explicity removed"
		apt-get -y --purge autoremove

		echo "FPP - Updating package list"
		apt-get update

		echo "FPP - Upgrading packages"
		apt-get -y upgrade

		# remove gnome keyring module config which causes pkcs11 warnings
		# when trying to do a git pull
		rm -f /etc/pkcs11/modules/gnome-keyring-module

		echo "FPP - Installing required packages"
		# Install 10 packages, then clean to lower total disk space required
		let packages=0
		for package in alsa-base alsa-utils arping avahi-daemon \
						zlib1g-dev libpcre3 libpcre3-dev libbz2-dev libssl-dev \
						avahi-discover avahi-utils bash-completion bc build-essential \
						bzip2 ca-certificates ccache curl device-tree-compiler \
						dh-autoreconf ethtool exfat-fuse fbi fbset file flite gdb \
						gdebi-core git i2c-tools ifplugd imagemagick less \
						libboost-dev libconvert-binary-c-perl \
						libdbus-glib-1-dev libdevice-serialport-perl libjs-jquery \
						libjs-jquery-ui libjson-perl libjsoncpp-dev libnet-bonjour-perl \
						libpam-smbpass libtagc0-dev libtest-nowarnings-perl locales \
						mp3info mpg123 mpg321 mplayer nano node ntp perlmagick \
						php5-cli php5-common php5-curl php5-fpm php5-mcrypt \
						php5-sqlite php-apc python-daemon python-smbus samba \
						samba-common-bin shellinabox sudo sysstat tcpdump usbmount vim \
						vim-common vorbis-tools vsftpd firmware-realtek gcc g++\
						network-manager dhcp-helper hostapd parprouted bridge-utils
		do
			apt-get -y install ${package}
			let packages=$((${packages}+1))
			if [ $packages -gt 10 ]; then
				let packages=0
				apt-get -y clean
			fi
		done

		echo "FPP - Cleaning up after installing packages"
		apt-get -y clean

		echo "FPP - Installing non-packaged Perl modules via App::cpanminus"
		curl -L https://cpanmin.us | perl - --sudo App::cpanminus
		echo "yes" | cpanm -fi Test::Tester File::Map Net::WebSocket::Server

		echo "FPP - Disabling any stock 'debian' user, use the 'fpp' user instead"
		sed -i -e "s/^debian:.*/debian:*:16372:0:99999:7:::/" /etc/shadow

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

		echo "FPP - Disabling dhcp-helper and hostapd from automatically starting"
		update-rc.d -f dhcp-helper remove
		update-rc.d -f hostapd remove

		if [ "x${OSVER}" == "xdebian_8" ]; then
			systemctl disable display-manager.service
		fi

		;;
	ubuntu_14.04)
		echo "FPP - Updating package list"
		apt-get update
		echo "FPP - Installing required packages"
		if [ "x${FPPPLATFORM}" = "xODROID" ]
		then
			echo "FPP - WARNING: This list may be incomplete, it needs to be updated to match the Pi/BBB Debian package install list"
			apt-get -y install avahi-discover fbi flite gdebi-core i2c-tools imagemagick libboost-dev libconvert-binary-c-perl libjson-perl libjsoncpp-dev libnet-bonjour-perl libpam-smbpass mp3info mpg123 perlmagick php5 php5-cli php5-common php-apc python-daemon python-smbus samba samba-common-bin shellinabox sysstat vorbis-tools vsftpd
		fi
		;;
	*)
		echo "FPP - Unknown distro"
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
		apt-get -y --force-yes install libcppunit-dev uuid-dev pkg-config libncurses5-dev libtool autoconf automake libmicrohttpd-dev protobuf-compiler python-protobuf libprotobuf-dev libprotoc-dev bison flex libftdi-dev libftdi1 libusb-1.0-0-dev liblo-dev
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
#		apt-get -y --force-yes install libcppunit-dev libcppunit-1.12-1 uuid-dev pkg-config libncurses5-dev libtool autoconf automake libmicrohttpd-dev protobuf-compiler python-protobuf libprotobuf-dev libprotoc-dev bison flex libftdi-dev libftdi1 libusb-1.0-0-dev liblo-dev
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
		echo "FPP - Updating firmware for Raspberry Pi install"
		#https://raw.githubusercontent.com/Hexxeh/rpi-update/master/rpi-update
		wget http://goo.gl/1BOfJ -O /usr/bin/rpi-update && chmod +x /usr/bin/rpi-update
		SKIP_WARNING=1 rpi-update 12f0636cd11ebd7ec189534147ea23ce4f702e90

		echo "FPP - Installing Pi-specific packages"
		apt-get -y install raspi-config

		# TODO: Shouldn't this stuff go somewhere besides the Pi section?  I
		# believe OLA is supported across most linux systems.  It can probably
		# go in another location.
		if $build_ola; then
			echo "FPP - Installing OLA from source"
			apt-get -y --force-yes install libcppunit-dev uuid-dev pkg-config libncurses5-dev libtool autoconf automake libmicrohttpd-dev protobuf-compiler python-protobuf libprotobuf-dev libprotoc-dev bison flex libftdi-dev libftdi1 libusb-1.0-0-dev liblo-dev
			apt-get -y clean
			git clone https://github.com/OpenLightingProject/ola.git /opt/ola
			(cd /opt/ola && autoreconf -i && ./configure --enable-rdm-tests --enable-python-libs && make && make install && ldconfig)
			rm -rf /opt/ola
		else
			echo "FPP - Installing OLA packages"
			apt-get -y --force-yes install libcppunit-dev uuid-dev pkg-config libncurses5-dev libtool autoconf automake libmicrohttpd-dev protobuf-compiler python-protobuf libprotobuf-dev libprotoc-dev bison flex libftdi-dev libftdi1 libusb-1.0-0-dev liblo-dev
			apt-get -y clean

			mkdir /tmp/deb
			cd /tmp/deb
			FILES="libola-dev_0.10.0-1_armhf.deb libola1_0.10.0-1_armhf.deb ola-python_0.0.10-1_all.deb ola-rdm-tests_0.0.10-1_all.deb ola_0.0.10-1_armhf.deb"
			for FILE in ${FILES}
			do
				# TODO Host the debs I built so we can use our packages
				# instaed of the broken ones in the OLA repo
				wget -nd http://www.bc2va.org/chris/tmp/fpp/deb/pi/${OSVER}/${FILE}
			done
			dpkg --unpack ${FILES}
			rm -f ${FILES}
		fi

		echo "FPP - Installing wiringPi"
		cd /opt/ && git clone git://git.drogon.net/wiringPi && cd /opt/wiringPi && ./build

		if $build_omxplayer; then
			echo "FPP - Building omxplayer from source with our patch"
			apt-get -y install subversion libidn11-dev libboost1.50-dev libfreetype6-dev libusb-1.0-0-dev libssh-dev libsmbclient-dev g++-4.7
			git clone https://github.com/popcornmix/omxplayer.git
			cd omxplayer
			git reset --hard 4d8ffd13153bfef2966671cb4fb484afeaf792a8
			wget -O- https://raw.githubusercontent.com/FalconChristmas/fpp/stage/external/omxplayer/FPP_omxplayer.diff | patch -p1
			./prepare-native-raspbian.sh
			make ffmpeg
			make
			tar xzpvf omxplayer-dist.tgz -C /
			cd ..
		else
			# TODO: need to test this binary on jessie
			echo "FPP - Installing patched omxplayer.bin for FPP MultiSync"
			apt-get -y install libssh-4
			wget -O- https://github.com/FalconChristmas/fpp-binaries/raw/master/Pi/omxplayer-dist.tgz | tar xzpv -C /
		fi

		echo "FPP - Disabling any stock 'pi' user, use the 'fpp' user instead"
		sed -i -e "s/^pi:.*/pi:*:16372:0:99999:7:::/" /etc/shadow

		echo "FPP - Disabling getty on onboard serial ttyAMA0"
		if [ "x${OSVER}" == "xdebian_7" ]; then
			sed -i "s@T0:23:respawn:/sbin/getty -L ttyAMA0@#T0:23:respawn:/sbin/getty -L ttyAMA0@" /etc/inittab
		elif [ "x${OSVER}" == "xdebian_8" ]; then
			systemctl disable serial-getty@ttyAMA0.service
		fi

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
		;;
	#TODO
	'CHIP')
		echo "FPP - Reinstall chip packages that were probably removed earlier from dependencies"
		for package in chip-exit chip-metapackage chip-configs chip-hwtest \
						chip-input-reset chip-power chip-theme
		do
			apt-get -y install ${package}
			let packages=$((${packages}+1))
			if [ $packages -gt 10 ]; then
				let packages=0
				apt-get -y clean
			fi
		done
		;;
	'ODROID')
		echo "FPP - Installing wiringPi"
		cd /opt/ && git clone https://github.com/hardkernel/wiringPi && cd /opt/wiringPi && ./build

		echo "FPP - Installing patched omxplayer.bin for FPP MultiSync"
		cp /usr/bin/omxplayer.bin /usr/bin/omxplayer.bin.orig
		wget -O /usr/bin/omxplayer.bin https://github.com/FalconChristmas/fpp-binaries/raw/master/Pi/omxplayer.bin
		;;
	'Debian')
		echo "FPP - Debian"
		;;
	*)
		echo "FPP - Unknown platform"
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
sed -i -e "s/.*listen.owner =.*/listen.owner = fpp/" /etc/php5/fpm/pool.d/www.conf
sed -i -e "s/.*listen.group =.*/listen.group = fpp/" /etc/php5/fpm/pool.d/www.conf
sed -i -e "s/.*listen.mode =.*/listen.mode = 0660/" /etc/php5/fpm/pool.d/www.conf

#######################################
echo "FPP - Allowing short tags in PHP"
sed -i -e "s/^short_open_tag.*/short_open_tag = On/" /etc/php5/cli/php.ini
sed -i -e "s/^short_open_tag.*/short_open_tag = On/" /etc/php5/fpm/php.ini

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
case "${FPPPLATFORM}" in
	'Raspberry Pi'|'BeagleBone Black')
		adduser fpp spi
		;;
esac
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
case "${OSVER}" in
	debian_7)
		service samba restart
		;;
	debian_8)
		systemctl restart smbd.service
		systemctl restart nmbd.service
		;;
esac

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
# Building nginx
echo "FPP - Building nginx webserver"
git clone https://github.com/wandenberg/nginx-push-stream-module.git
wget http://nginx.org/download/nginx-1.8.1.tar.gz
tar xzvf nginx-1.8.1.tar.gz
cd nginx-1.8.1/
./configure --add-module=../nginx-push-stream-module
make
make install

#######################################
echo "FPP - Configuring nginx webserver"
# Comment out the default server section
sed -i.orig '/^\s*location/,/^\s*}/s/^/#/g;/^\s*server/,/^\s*}/s/^/#/g;s/##/#/g' /usr/local/nginx/conf/nginx.conf
# Add an include of our server configuration
sed -i -e '/^\s*http\s*{/a\    include /etc/fpp_nginx.conf;\n' /usr/local/nginx/conf/nginx.conf
sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" < ${FPPDIR}/etc/nginx.conf > /etc/fpp_nginx.conf
# Set user to fpp
sed -i -e 's/^\s*\#\?\s*user\(\s*\)[^;]*/user\1fpp/' /usr/local/nginx/conf/nginx.conf
# Ensure pid matches our systemd service file
sed -i -e 's/^\s*\#\?\s*pid\(\s*\)[^;]*/pid\1logs\/nginx.pid/' /usr/local/nginx/conf/nginx.conf

case "${OSVER}" in
	debian_7)
		cp ${FPPDIR}/scripts/nginx /etc/init.d/
		update-rc.d nginx defaults
		;;
	debian_8)
		cp ${FPPDIR}/scripts/nginx.service /lib/systemd/system/
		systemctl enable nginx.service
		;;
esac

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
