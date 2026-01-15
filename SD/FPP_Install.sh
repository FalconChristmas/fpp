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
# FPP installs created by this script are not supported.  Please do not log
# bugs against these installs.  FPP is only supported when using FPP OS images
# installed on Pi/Beagles.
#
# Make sure your system is connected to the internet via a wired connection
# before running this script
#
#############################################################################
# To use this script, download the latest copy from github and run it as
# root on the system where you want to install FPP:
#
# curl -o ./FPP_Install.sh https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/FPP_Install.sh
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
#       latest OS images for the Raspberry Pi and BeagleBone Black.  See
#       appropriate README for the exact image that is used.
#
#       Raspberry Pi
#           - Login/Password
#             - pi/raspberry
#
#       BeagleBone Black
#           - Login/Password
#             - debian/temppwd
#
#       Other OS images may work with this install script and FPP on the
#       Pi and BBB platforms, but these are the images we are currently
#       targetting for support.
#
#############################################################################
# Other platforms which may be functioning:
#
# NOTE: FPP Development is based on the latest Debian Bookworm images, so
#       hardware which does not support Bookworm may have issues.
#
#############################################################################
FPPBRANCH=${FPPBRANCH:-"master"}
FPPIMAGEVER="2026-01"
FPPCFGVER="102"
FPPPLATFORM="UNKNOWN"
FPPDIR=/opt/fpp
FPPUSER=fpp
FPPHOME=/home/${FPPUSER}
OSVER="UNKNOWN"

# Make sure the sbin directories are on the path as we will
# need the adduser/addgroup/ldconfig/a2enmod/etc... commands
PATH=$PATH:/usr/sbin:/sbin

# CPU Count
CPUS=$(grep "^processor" /proc/cpuinfo | wc -l)
if [[ ${CPUS} -gt 1 ]]; then
    MEMORY=$(grep MemTot /proc/meminfo | awk '{print $2}')
    if [[ ${MEMORY} -lt 425000 ]]; then
        # very limited memory, only use one core or we'll fail or
        # will be very slow as we constantly swap in/out
        CPUS=1
    elif [[ ${MEMORY} -lt 512000 ]]; then
        SWAPTOTAL=$(grep SwapTotal /proc/meminfo | awk '{print $2}')
        # Limited memory, if we have some swap, we'll go ahead with -j 2
        # otherwise we'll need to stick with -j 1 or we run out of memory
        if [[ ${SWAPTOTAL} -gt 49000 ]]; then
            CPUS=2
        else
            CPUS=1
        fi
    fi
fi
        
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
        USNODATE=$(curl -I --connect-timeout 10 --max-time 10 --retry 2 --retry-max-time 10 -f -v --silent https://nist.time.gov/ 2>&1 \ | grep '< Date' | sed -e 's/< Date: //')
        if  [ "x${USNODATE}" != "x" ]
        then
        USNOSECS=$(date -d "${USNODATE}" +%s)
        else 
        USNOSECS=""
        fi

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
clone_fpp=true
build_vlc=false
skip_apt_install=false
desktop=true
isimage=false;


MODEL=""
if [ -f /proc/device-tree/model ]; then
    MODEL=$(tr -d '\0' < /proc/device-tree/model)
fi
# Attempt to detect the platform we are installing on
if [ "x${OSID}" = "xraspbian" ]
then
	FPPPLATFORM="Raspberry Pi"
	OSVER="debian_${VERSION_ID}"
    isimage=true
    desktop=false
elif [ -e "/sys/class/leds/beaglebone:green:usr1" ]
then
    ARCH=$(uname -m)
if [ "$ARCH" == "aarch64" ]; then
    FPPPLATFORM="BeagleBone 64"
else
	FPPPLATFORM="BeagleBone Black"
fi
    isimage=true
    desktop=false
elif [ -f "/etc/armbian-release" ]
then
    FPPPLATFORM="Armbian"
    isimage=true
    desktop=false
elif [[ $PRETTY_NAME == *"Armbian"* ]]
then
    FPPPLATFORM="Armbian"
    isimage=true
    desktop=false
elif [ "x${OSID}" = "xdebian" ]
then
	FPPPLATFORM="Debian"
elif [ "x${OSID}" = "xubuntu" ]
then
	FPPPLATFORM="Ubuntu"
elif [ "x${OSID}" = "xlinuxmint" ]
then
	FPPPLATFORM="Mint"
elif [ "x${OSID}" = "xfedora" ]
then
    FPPPLATFORM="Fedora"
else
	FPPPLATFORM="UNKNOWN"
fi


while [ -n "$1" ]; do
    case $1 in
        --skip-clone)
            clone_fpp=false
            shift
            ;;
        --skip-vlc)
            build_vlc=false
            shift
            ;;
        --build-vlc)
            build_vlc=true
            shift
            ;;
        --skip-apt-install)
            skip_apt_install=true
            shift
            ;;
        --img)
            desktop=false
            isimage=true
            shift
            ;;
        --no-img)
            desktop=true
            isimage=false
            shift
            ;;
        --branch)
            FPPBRANCH=$2
            shift
            shift
            ;;
        *)
            echo "Unknown option $1" >&2
            exit 1
            ;;
    esac
done

checkTimeAgainstUSNO

#############################################################################
echo "============================================================"
echo "FPP Image Version: v${FPPIMAGEVER}"
echo "FPP Directory    : ${FPPDIR}"
echo "FPP Branch       : ${FPPBRANCH}"
echo "Operating System : ${PRETTY_NAME}"
echo "Platform         : ${FPPPLATFORM}"
echo "OS Version       : ${OSVER}"
echo "OS ID            : ${OSID}"
echo "Image            : ${isimage}"
echo "============================================================"
#############################################################################

echo ""
echo "Notes:"
echo "- Does this system have internet access to install packages and FPP?"
echo ""
echo "WARNINGS:"
echo "- This install expects to be run on a clean freshly-installed system."
echo "  The script is not currently designed to be re-run multiple times."
if $isimage; then
    echo "- This installer will take over your system.  It will disable any"
    echo "  existing 'pi' or 'debian' user and create a '${FPPUSER}' user.  If the system"
    echo "  has an empty root password, remote root login will be disabled."
fi
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
	rm -f /etc/fpp/config_version
    rm -f /etc/fpp/platform
    rm -f /etc/fpp/rfs_version
    rm -f /etc/fpp/desktop
fi

#######################################
# Create /etc/fpp directory and contents
echo "FPP - Creating /etc/fpp and contents"
mkdir /etc/fpp
echo "${FPPPLATFORM}" > /etc/fpp/platform
echo "v${FPPIMAGEVER}" > /etc/fpp/rfs_version
echo "${FPPCFGVER}" > /etc/fpp/config_version
if $desktop; then
    echo "1" > /etc/fpp/desktop
else
# Bunch of configuration we wont do if installing onto desktop
# We shouldn't set the hostname or muck with the issue files, keyboard, etc...

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
cp /etc/issue.new /etc/issue.net
rm /etc/issue.new

head -1 /etc/issue > /etc/issue.new
cat >> /etc/issue.new <<EOF

Falcon Player OS Image v${FPPIMAGEVER}
My IP address: \4

FPP is configured from a web browser. Point your browser at:
http://\4 , http://\n.local , or http://\n 

EOF
cp /etc/issue.new /etc/issue
rm /etc/issue.new


#######################################
# Setup for U.S. users mainly
echo "FPP - Setting US keyboard layout and locale"
sed -i "s/XKBLAYOUT=".*"/XKBLAYOUT="us"/" /etc/default/keyboard
echo "LANG=en_US.UTF-8" > /etc/default/locale

# end of if desktop
fi


#######################################
# Make sure /opt exists
echo "FPP - Checking for existence of /opt"
cd /opt 2> /dev/null || mkdir /opt


#######################################
# Make sure dependencies are installed
# Set noninteractive install to skip prompt about restarting services
export DEBIAN_FRONTEND=noninteractive

case "${OSVER}" in
	debian_12 | debian_13 | ubuntu_24.* | linuxmint_21)

        # Stop unattended-upgrades as it can hold a lock on the apt repository
        systemctl stop unattended-upgrades

        #remove a bunch of packages that aren't neeeded, free's up space
        PACKAGE_REMOVE="nginx nginx-full nginx-common  triggerhappy pocketsphinx-en-us guile-2.2-libs \
            gfortran glib-networking libxmuu1 xauth network-manager dhcpcd5 fake-hwclock ifupdown isc-dhcp-client isc-dhcp-common openresolv \
            unattended-upgrades packagekit iwd"
        if [ "$FPPPLATFORM" == "BeagleBone 64" -o "$FPPPLATFORM" == "BeagleBone Black" ]; then
            PACKAGE_REMOVE="$PACKAGE_REMOVE nodejs bb-node-red-installer mender-client bb-code-server"
        fi
        if $desktop; then
            #don't remove anything from a desktop
            PACKAGE_REMOVE=""
        fi
        
        if $skip_apt_install; then
            PACKAGE_REMOVE=""
        fi
        
        if [ "x${PACKAGE_REMOVE}" != "x" ]; then
            # Need to make sure there is configuration for eth0 or uninstalling dhcpclient will cause network to drop
            rm -f /etc/systemd/network/50-default.network
            curl -o /etc/systemd/network/50-default.network https://raw.githubusercontent.com/FalconChristmas/fpp/master/etc/systemd/network/50-default.network
            
            apt-get install -y systemd-resolved
            systemctl reload systemd-networkd
            apt-get remove -y --purge --autoremove --allow-change-held-packages ${PACKAGE_REMOVE}
            
            systemctl unmask systemd-networkd
            systemctl unmask systemd-resolved

            systemctl restart systemd-networkd
            systemctl restart systemd-resolved

            systemctl enable systemd-networkd
            systemctl enable systemd-resolved

            echo "FPP - Sleeping 5 seconds to make sure network is available"
            sleep 5
        fi
        
		echo "FPP - Removing anything left that wasn't explicity removed"
		apt-get -y --purge autoremove

		echo "FPP - Updating package list"
		apt-get update

		echo "FPP - Upgrading apt if necessary"
		apt-get install --only-upgrade apt

		echo "FPP - Sleeping 5 seconds to make sure any apt upgrade is quiesced"
		sleep 5

		echo "FPP - Upgrading other installed packages"
		apt-get -y upgrade
  
        echo "FPP - Cleanup caches"
        apt-get -y clean
        apt-get -y --purge autoremove
        apt-get -y clean

		# remove gnome keyring module config which causes pkcs11 warnings
		# when trying to do a git pull
		rm -f /etc/pkcs11/modules/gnome-keyring-module

		echo "FPP - Installing required packages"
		# Install 10 packages, then clean to lower total disk space required
  
        PHPVER=""
        ACTUAL_PHPVER="7.4"
        if [ "${OSVER}" == "ubuntu_22.04" -o "${OSVER}" == "linuxmint_21" ]; then
            PHPVER="7.4"
            echo "FPP - Forcing PHP 7.4"
            apt install software-properties-common apt-transport-https -y
            add-apt-repository ppa:ondrej/php -y
            apt-get -y update
            apt-get -y upgrade
        fi
        if [ "${OSVER}" == "ubuntu_22.10" ]; then
            ACTUAL_PHPVER="8.1"
        fi
        if [ "${OSVER}" == "debian_12" ]; then
            ACTUAL_PHPVER="8.2"
            PHPVER="8.2"
        fi
        if [ "${OSVER}" == "debian_13" ]; then
            ACTUAL_PHPVER="8.4"
            PHPVER="8.4"
        fi

        #########################################
        # WARNING
        # ---------------------------------------
        # If you modify this file, be sure to update Docker/Dockerfile
        #########################################

        PACKAGE_LIST="alsa-utils arping avahi-daemon avahi-utils locales nano net-tools distcc \
                      apache2 apache2-bin apache2-data apache2-utils libavahi-client-dev \
                      bc bash-completion btrfs-progs exfat-fuse lsof ethtool curl zip unzip bzip2 wireless-tools dos2unix \
                      fbi fbset file flite ca-certificates lshw gettext wget iproute2 fswatch \
                      build-essential ffmpeg gcc g++ gdb vim vim-common bison flex device-tree-compiler dh-autoreconf \
                      git hdparm i2c-tools jq less sysstat tcpdump time usbutils usb-modeswitch \
                      samba rsync sudo shellinabox dnsmasq hostapd vsftpd sqlite3 at haveged samba samba-common-bin \
                      mp3info exim4 mailutils dhcp-helper parprouted bridge-utils libiio-utils \
                      php${PHPVER} php${PHPVER}-cli php${PHPVER}-fpm php${PHPVER}-common php${PHPVER}-curl php-pear \
                      php${PHPVER}-bcmath php${PHPVER}-sqlite3 php${PHPVER}-zip php${PHPVER}-xml ccache \
                      libavcodec-dev libavformat-dev libswresample-dev libswscale-dev libavdevice-dev libavfilter-dev libtag1-dev \
                      vorbis-tools libgraphicsmagick++1-dev graphicsmagick-libmagick-dev-compat libmicrohttpd-dev \
                      gettext apt-utils x265 libtheora-dev libvorbis-dev libx265-dev iputils-ping mp3gain \
                      libmosquitto-dev mosquitto-clients mosquitto libzstd-dev lzma zstd gpiod libgpiod-dev libjsoncpp-dev libcurl4-openssl-dev \
                      fonts-freefont-ttf flex bison pkg-config libasound2-dev mesa-common-dev qrencode libusb-1.0-0-dev \
                      flex bison pkg-config libasound2-dev python3-setuptools libssl-dev libtool bsdextrautils iw rsyslog tzdata libsystemd-dev"

        if [ "$FPPPLATFORM" == "Raspberry Pi" -o "$FPPPLATFORM" == "BeagleBone Black"  -o "$FPPPLATFORM" == "BeagleBone 64" ]; then
            PACKAGE_LIST="$PACKAGE_LIST firmware-realtek firmware-atheros firmware-ralink firmware-brcm80211 firmware-iwlwifi firmware-libertas firmware-zd1211 firmware-ti-connectivity zram-tools"
            if [ "$FPPPLATFORM" == "Raspberry Pi" ]; then
                PACKAGE_LIST="$PACKAGE_LIST libva-dev smartmontools edid-decode"
            fi
            if [ "$FPPPLATFORM" == "BeagleBone Black"  -o "$FPPPLATFORM" == "BeagleBone 64" ]; then
                PACKAGE_LIST="$PACKAGE_LIST ti-pru-cgt-v2.3"
            fi
        else
            PACKAGE_LIST="$PACKAGE_LIST libva-dev smartmontools"
        fi
        if $isimage; then
            PACKAGE_LIST="$PACKAGE_LIST networkd-dispatcher"
        fi
        if ! $build_vlc; then
            PACKAGE_LIST="$PACKAGE_LIST vlc libvlc-dev"
            if [ "${OSVER}" == "debian_13" ]; then
                PACKAGE_LIST="$PACKAGE_LIST vlc-plugin-pipewire"
            fi
        fi
        if [ "${OSVER}" == "debian_12" ]; then
            PACKAGE_LIST="$PACKAGE_LIST python3-distutils"
        fi
        if [ "${OSVER}" == "debian_13" ]; then
            PACKAGE_LIST="$PACKAGE_LIST ntpsec pipewire"
        else
            PACKAGE_LIST="$PACKAGE_LIST ntp ifplugd libfmt9"
        fi
        if [ "$FPPPLATFORM" == "BeagleBone 64" ]; then
            PACKAGE_LIST="$PACKAGE_LIST cpufrequtils"
        fi

        
        if $skip_apt_install; then
            echo "skipping apt install because skpt_apt_install == $skip_apt_install"
            PACKAGE_LIST=""
        else
            echo "--------------------------"
            echo "INSTALLING $PACKAGE_LIST"
            echo "--------------------------"
            apt-get -y -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" install ${PACKAGE_LIST}
        fi
        
        echo "FPP - Installing libhttpserver 0.19.0"
        (cd /opt/ && git clone https://github.com/etr/libhttpserver && cd libhttpserver && git checkout 0.19.0 && ./bootstrap && autoupdate && ./bootstrap && mkdir build && cd build && ../configure --prefix=/usr --disable-examples && make -j ${CPUS} && make install && cd /opt/ && rm -rf /opt/libhttpserver)
        
        echo "FPP - Installing libkms++"
        (cd /opt/ && git clone https://github.com/tomba/kmsxx && cd kmsxx && git checkout 0f18e6d0616b597fc32bba78b38dfc5c922ec9a4 && apt-get install -y meson cmake libfmt-dev && meson setup build --prefix=/usr -Dpykms=disabled && ninja -j ${CPUS} -C build install && cd /opt/ && rm -rf kmsxx && apt-get remove -y --purge --autoremove meson cmake libfmt-dev && ccache -C)
        
        if $isimage; then
            apt-get -y -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" install systemd wpasupplicant
        fi
        echo "FPP - Cleaning up after installing packages"
        apt-get -y clean


        echo "FPP - Configuring shellinabox to use /var/tmp"
        echo "SHELLINABOX_DATADIR=/var/tmp/" >> /etc/default/shellinabox
        sed -i -e "s/SHELLINABOX_ARGS.*/SHELLINABOX_ARGS=\"--no-beep -t\"/" /etc/default/shellinabox


		if [ -f /bin/systemctl ]
		then
			echo "FPP - Disabling unneeded/unwanted services"
			systemctl disable bonescript.socket
			systemctl disable bonescript-autorun.service
			systemctl disable console-kit-daemon.service
			systemctl disable cloud9.socket
            systemctl disable beagle-flasher-init-shutdown.service
		fi

        if $isimage; then
            echo "FPP - Disabling GUI"
            systemctl disable gdm
            systemctl disable gdm3
            systemctl disable lightdm
            systemctl disable wdm
            systemctl disable xdm
            systemctl disable display-manager.service
            
            echo "FPP - Disabling dhcp-helper and hostapd from automatically starting"
            systemctl disable dhcp-helper
            systemctl disable hostapd
            
            echo "FPP - Disabling the auto-upgrade services"
            systemctl disable apt-daily.service || true
            systemctl disable apt-daily.timer || true
            systemctl disable apt-daily-upgrade.service || true
            systemctl disable apt-daily-upgrade.timer || true
            
            echo "FPP - Enabling systemd-networkd"
            # clean out the links to /dev/null so that we can enable systemd-networkd
            rm -f /etc/systemd/network/99*
            rm -f /etc/systemd/network/20*
            rm -f /etc/systemd/network/73*
            rm -f /etc/systemd/network/eth*
            rm -f /etc/systemd/network/wlan*
            rm -f /etc/systemd/network/SoftAp*
            rm -rf /etc/wpa_supplicant/wpa_supplicant-wl*
            if [ ! -f /etc/systemd/network/50-default.network ]; then
                # Need to make sure there is configuration for eth0 or no connection will be
                # setup after a reboot
                curl -o /etc/systemd/network/50-default.network https://raw.githubusercontent.com/FalconChristmas/fpp/master/etc/systemd/network/50-default.network
            fi
            # make sure we end up with eth0/wlan0 instead of enx#### wlx#### naming for now
            ln -s /dev/null /etc/systemd/network/99-default.link
            ln -s /dev/null /etc/systemd/network/73-usb-net-by-mac.link
            systemctl enable systemd-networkd
            systemctl disable systemd-networkd-wait-online.service
            systemctl enable systemd-resolved
            systemctl enable networkd-dispatcher

            # if systemd hasn't created a new resolv.conf, don't replace it yet
            if [ -f /run/systemd/resolve/resolv.conf ]; then
                rm -f /etc/resolv.conf
                ln -s /run/systemd/resolve/resolv.conf /etc/resolv.conf
            fi

            #remove some things that were installed (not sure why)
            apt-get remove -y --purge --autoremove --allow-change-held-packages pocketsphinx-en-us

            if [ "$FPPPLATFORM" == "Raspberry Pi" ]; then
                echo "FPP - Fixing haveged"
                mkdir -p /etc/systemd/system/haveged.service.d
                echo -e '[Service]\nSystemCallFilter=uname' > /etc/systemd/system/haveged.service.d/syscall.conf
            
                echo "FPP - Applying updates to allow optional boot from USB on Pi 4 (and up)"

                # make sure the label on p1 is "boot" and p2 is rootfs
                fatlabel /dev/mmcblk0p1 boot
                e2label /dev/mmcblk0p2 rootfs

                BOOTDIR="/boot"
                if [ "${OSVER}" == "debian_12" ]; then
                    BOOTDIR="/boot/firmware"
                fi
                if [ "${OSVER}" == "debian_13" ]; then
                    BOOTDIR="/boot/firmware"
                fi


                # Update /etc/fstab to use PARTUUID for / and /boot
                BOOTLINE=$(grep "^[^#].* /boot[a-z/]* " /etc/fstab | sed -e "s;^[^#].* /boot[a-z/]* ;LABEL=boot ${BOOTDIR} ;")
                ROOTLINE=$(grep "^[^#].* /  " /etc/fstab | sed -e "s;^[^#].* / ;LABEL=rootfs / ;")
                echo "    - Updating /etc/fstab"
                sed -i -e "s;^[^#].* /boot[a-z/]* .*;${BOOTLINE};" -e "s;^[^#].* / .*;${ROOTLINE};" /etc/fstab

                # Create two different cmdline.txt files for USB and SD boot
                echo "    - Creating ${BOOTDIR}/cmdline.* configs"
                sed -i -e "s; root=PARTUUID=[0-9A-Za-z\-]* ; root=/dev/mmcblk0p2 ;g" ${BOOTDIR}/cmdline.txt
                cat ${BOOTDIR}/cmdline.txt | sed -e "s/root=\/dev\/[a-zA-Z0-9]*\([0-9]\) /root=\/dev\/sda\1 /" > ${BOOTDIR}/cmdline.usb
                cat ${BOOTDIR}/cmdline.txt | sed -e "s/root=\/dev\/[a-zA-Z0-9]*\([0-9]\) /root=\/dev\/mmcblk0p\1 /" > ${BOOTDIR}/cmdline.sd

                # Create two batch files to switch back and forth between USB and SD boot
                echo "    - Creating ${BOOTDIR}/boot_*.bat scripts"
                cat > ${BOOTDIR}/boot_usb.bat <<EOF
@echo off
rem This script will switch ${BOOTDIR}/cmdline.txt to boot from a USB
rem drive which will show up as /dev/sda inside the OS.

@echo Copying USB boot config file into place.
copy /y cmdline.usb cmdline.txt
@echo.

@echo Copy complete.
@echo.

set /p DUMMY=Hit ENTER to continue...
EOF

                cat > ${BOOTDIR}/boot_sd.bat <<EOF
@echo off
rem This script will switch ${BOOTDIR}/cmdline.txt to boot from a SD
rem card which will show up as /dev/mmcblk0 inside the OS.

@echo Copying (micro)SD boot config file into place.
copy /y cmdline.sd cmdline.txt
@echo.

@echo Copy complete.
@echo.

set /p DUMMY=Hit ENTER to continue...
EOF

            fi
        fi

		;;
	*)
		echo "FPP - Unknown distro"
		;;
esac

if [ ! -f "/usr/include/SDL2/SDL.h" ]; then
    # we don't want to "apt-get install libsdl2-dev" as that brings in wayland, a bunch
    # of X libraries, mesa, some OpenGL libs, etc... A bunch of things we DON'T need
    # Thus, we'll grab the .deb file, modify it to remove those deps, and install
    echo "FPP - Installing SDL2"
    cd /tmp
    apt-get download libsdl2-dev
    mkdir deb
    dpkg-deb -R ./libsdl2*.deb  deb
    sed -i -e "s/Version\(.*\)+\(.*\)/Version\1~fpp/g" deb/DEBIAN/control
    sed -i -e "s/Depends: \(.*\)/Depends: libsdl2-2.0-0, libasound2-dev, libdbus-1-dev, libsndio-dev/g" deb/DEBIAN/control
    dpkg-deb -b deb ./libsdl2-dev.deb
    apt -y install ./libsdl2-dev.deb
    apt-mark hold libsdl2-dev
    apt-get clean
fi
 
#######################################
# Platform-specific config
case "${FPPPLATFORM}" in
	'BeagleBone Black')

        # need to blacklist the gyroscope and barometer on the SanCloud enhanced or it consumes some pins
        echo "blacklist st_pressure_spi" > /etc/modprobe.d/blacklist-gyro.conf
        echo "blacklist st_pressure_i2c" >> /etc/modprobe.d/blacklist-gyro.conf
        echo "blacklist st_pressure" >> /etc/modprobe.d/blacklist-gyro.conf
        echo "blacklist inv_mpu6050_i2c" >> /etc/modprobe.d/blacklist-gyro.conf
        echo "blacklist st_sensors_spi" >> /etc/modprobe.d/blacklist-gyro.conf
        echo "blacklist st_sensors_i2c" >> /etc/modprobe.d/blacklist-gyro.conf
        echo "blacklist inv_mpu6050" >> /etc/modprobe.d/blacklist-gyro.conf
        echo "blacklist st_sensors" >> /etc/modprobe.d/blacklist-gyro.conf
        echo "blacklist spidev" > /etc/modprobe.d/blacklist-spidev.conf

        # need to blacklist the bluetooth on BBG Gateway or it tends to crash the kernel, we don't need it
        echo "blacklist btusb" > /etc/modprobe.d/blacklist-bluetooth.conf
        echo "blacklist bluetooth" >> /etc/modprobe.d/blacklist-bluetooth.conf
        echo "blacklist hci_uart" >> /etc/modprobe.d/blacklist-bluetooth.conf
        echo "blacklist bnep" >> /etc/modprobe.d/blacklist-bluetooth.conf

        rm -f  /etc/modules-load.d/network.conf

        systemctl disable keyboard-setup
        systemctl disable unattended-upgrades
        systemctl disable resize_filesystem
        systemctl disable console-setup
		;;
    'BeagleBone 64')
        systemctl disable keyboard-setup
        systemctl disable unattended-upgrades
        systemctl disable mender-client
        systemctl disable resize_filesystem
        systemctl disable console-setup
        systemctl disable samba-ad-dc

        echo "FPP - Adding required modules to modules-load to speed up boot"
        echo "snd_pcm" >> /etc/modules-load.d/modules.conf
        echo "snd_timer" >> /etc/modules-load.d/modules.conf
        echo "snd" >> /etc/modules-load.d/modules.conf
        echo "soundcore" >> /etc/modules-load.d/modules.conf
        echo "irq_pruss_intc" >> /etc/modules-load.d/modules.conf
        echo "pru_rproc" >> /etc/modules-load.d/modules.conf
        echo "cpufreq_dt" >> /etc/modules-load.d/modules.conf
        echo "rti_wdt" >> /etc/modules-load.d/modules.conf
        echo "at24" >> /etc/modules-load.d/modules.conf
        echo "ad7291" >> /etc/modules-load.d/modules.conf
        echo "pruss" >> /etc/modules-load.d/modules.conf
        echo "omap_mailbox" >> /etc/modules-load.d/modules.conf
        echo "loop" >> /etc/modules-load.d/modules.conf
        echo "efi_pstore" >> /etc/modules-load.d/modules.conf
        echo "dm_mod" >> /etc/modules-load.d/modules.conf
        echo "ip_tables" >> /etc/modules-load.d/modules.conf
        ;;

	'Raspberry Pi')
		echo "FPP - Updating firmware for Raspberry Pi install"
        apt-get dist-upgrade -y

		echo "FPP - Installing Pi-specific packages"
		apt-get -y install raspi-config

        if $isimage; then
            BOOTDIR="/boot"
            if [ "${OSVER}" == "debian_12" ]; then
                BOOTDIR="/boot/firmware"
            fi
            if [ "${OSVER}" == "debian_13" ]; then
                BOOTDIR="/boot/firmware"
            fi

        
            echo "FPP - Disabling stock users (pi, odroid, debian), use the '${FPPUSER}' user instead"
            sed -i -e "s/^pi:.*/pi:*:16372:0:99999:7:::/" /etc/shadow
            sed -i -e "s/^odroid:.*/odroid:*:16372:0:99999:7:::/" /etc/shadow
            sed -i -e "s/^debian:.*/debian:*:16372:0:99999:7:::/" /etc/shadow

            echo "FPP - Disabling the hdmi force hotplug setting"
            sed -i -e "s/hdmi_force_hotplug/#hdmi_force_hotplug/" ${BOOTDIR}/config.txt
                        
            echo "FPP - Disabling Camera AutoDetect"
            sed -i -e "s/camera_auto_detect/#camera_auto_detect/" ${BOOTDIR}/config.txt

            echo "FPP - Adding required modules to modules-load to speed up boot"
            echo "i2c_dev" >> /etc/modules-load.d/modules.conf
            echo "spidev" >> /etc/modules-load.d/modules.conf
            echo "at24" >> /etc/modules-load.d/modules.conf
            echo "lm75" >> /etc/modules-load.d/modules.conf
            echo "snd_bcm2835" >> /etc/modules-load.d/modules.conf
            echo "bcm2835_codec" >> /etc/modules-load.d/modules.conf
            echo "snd_usb_audio" >> /etc/modules-load.d/modules.conf

            echo "[all]">> ${BOOTDIR}/config.txt            
            echo "FPP - Enabling SPI in device tree"
            echo >> ${BOOTDIR}/config.txt
            echo "# Enable SPI in device tree" >> ${BOOTDIR}/config.txt
            echo "dtparam=spi=on" >> ${BOOTDIR}/config.txt
            echo >> ${BOOTDIR}/config.txt
            echo "# Enable PCIe for NVME storage" >> ${BOOTDIR}/config.txt
            echo "dtparam=pciex1_gen=3" >> ${BOOTDIR}/config.txt
            echo >> ${BOOTDIR}/config.txt
            echo "# Enable Cape Specific Overlays" >> ${BOOTDIR}/config.txt
            echo "dtparam=fpp-cape-overlay" >> ${BOOTDIR}/config.txt
            echo >> ${BOOTDIR}/config.txt

            echo "FPP - Updating SPI buffer size and enabling HDMI audio devices"
            sed -i 's/$/ spidev.bufsiz=102400 snd_bcm2835.enable_headphones=1/' ${BOOTDIR}/cmdline.txt

            echo "FPP - Updating root partition device"
            sed -i 's/root=PARTUUID=[A-Fa-f0-9-]* /root=\/dev\/mmcblk0p2 /g' ${BOOTDIR}/cmdline.txt
            sed -i 's/PARTUUID=[A-Fa-f0-9]*-01/\/dev\/mmcblk0p1/g' /etc/fstab
            sed -i 's/PARTUUID=[A-Fa-f0-9]*-02/\/dev\/mmcblk0p2/g' /etc/fstab

            echo "FPP - Disabling fancy network interface names"
            sed -i -e 's/rootwait/rootwait net.ifnames=0 /' ${BOOTDIR}/cmdline.txt

            echo "# Enable I2C in device tree" >> ${BOOTDIR}/config.txt
            echo "dtparam=i2c_arm=on,i2c_arm_baudrate=400000" >> ${BOOTDIR}/config.txt
            echo >> ${BOOTDIR}/config.txt

            echo "# Setting kernel scaling framebuffer method" >> ${BOOTDIR}/config.txt
            echo "scaling_kernel=8" >> ${BOOTDIR}/config.txt
            echo >> ${BOOTDIR}/config.txt

            echo "# Enable audio" >> ${BOOTDIR}/config.txt
            echo "dtparam=audio=on" >> ${BOOTDIR}/config.txt
            echo >> ${BOOTDIR}/config.txt

            echo "# Allow more current through USB" >> ${BOOTDIR}/config.txt
            echo "max_usb_current=1" >> ${BOOTDIR}/config.txt
            echo >> ${BOOTDIR}/config.txt

            echo "# Setup UART clock to allow DMX output" >> ${BOOTDIR}/config.txt
            echo "init_uart_clock=16000000" >> ${BOOTDIR}/config.txt
            echo >> ${BOOTDIR}/config.txt

            echo "# Swap Pi 3 and Zero W UARTs with BT" >> ${BOOTDIR}/config.txt
            echo "dtoverlay=miniuart-bt" >> ${BOOTDIR}/config.txt
            echo >> ${BOOTDIR}/config.txt

            echo "# Model Specific configuration" >>  ${BOOTDIR}/config.txt
            echo "# GPU memory set to 128 to deal with error in omxplayer with hi-def videos" >> ${BOOTDIR}/config.txt
            echo "[pi5]" >> ${BOOTDIR}/config.txt
            echo "gpu_mem=256" >> ${BOOTDIR}/config.txt
            echo "dtparam=uart0=on" >> ${BOOTDIR}/config.txt
            echo "[pi4]" >> ${BOOTDIR}/config.txt
            echo "gpu_mem=256" >> ${BOOTDIR}/config.txt
            echo "[pi3]" >> ${BOOTDIR}/config.txt
            echo "gpu_mem=128" >> ${BOOTDIR}/config.txt
            echo "[pi0]" >> ${BOOTDIR}/config.txt
            echo "gpu_mem=64" >> ${BOOTDIR}/config.txt
            echo "[pi02]" >> ${BOOTDIR}/config.txt
            echo "gpu_mem=128" >> ${BOOTDIR}/config.txt
            echo "[pi1]" >> ${BOOTDIR}/config.txt
            echo "gpu_mem=64" >> ${BOOTDIR}/config.txt
            echo "[pi2]" >> ${BOOTDIR}/config.txt
            echo "gpu_mem=64" >> ${BOOTDIR}/config.txt
            echo "" >> ${BOOTDIR}/config.txt
            echo "[all]" >> ${BOOTDIR}/config.txt
            echo "" >> ${BOOTDIR}/config.txt

            echo "FPP - Freeing up more space by removing unnecessary packages"
            apt-get -y purge wolfram-engine sonic-pi minecraft-pi firmware-iwlwifi libglusterfs0 mesa-va-drivers mesa-vdpau-drivers mesa-vulkan-drivers mkvtoolnix ncurses-term poppler-data va-driver-all librados2 libcephfs2
            apt-get -y --purge autoremove

            echo "FPP - Make things cleaner by removing configuration from unnecessary packages"
            dpkg --get-selections | grep deinstall | while read package deinstall; do
                apt-get -y purge $package
            done

            echo "FPP - Disabling Swap to save SD card"
            systemctl disable dphys-swapfile
            systemctl disable smartmontools

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

            cat <<-EOF > /etc/dnsmasq.d/usb.conf
interface=usb0
interface=usb1
port=53
dhcp-authoritative
domain-needed
bogus-priv
expand-hosts
cache-size=2048
dhcp-range=usb0,192.168.7.1,192.168.7.1,2m
dhcp-range=usb1,192.168.6.1,192.168.6.1,2m
listen-address=127.0.0.1
listen-address=192.168.7.2
listen-address=192.168.6.2
dhcp-option=usb0,3
dhcp-option=usb0,6
dhcp-option=usb1,3
dhcp-option=usb1,6
dhcp-leasefile=/var/run/dnsmasq.leases
EOF

            sed -i -e "s/^IGNORE_RESOLVCONF.*/IGNORE_RESOLVCONF=yes/g" /etc/default/dnsmasq
            sed -i -e "s/#IGNORE_RESOLVCONF.*/IGNORE_RESOLVCONF=yes/g" /etc/default/dnsmasq
            echo "DNSMASQ_EXCEPT=lo" >> /etc/default/dnsmasq

            echo "FPP - Removing extraneous blacklisted modules"
            rm -f /etc/modprobe.d/blacklist-*8192cu.conf
            rm -f /etc/modprobe.d/blacklist-*8xxxu.conf
            
            # this causes a symlink from gpiochip4 -> gpiochip0 which then prevents a gpio expander that would be
            # gpiochip4 from actually loading and getting a dev entry
            rm -f /usr/lib/udev/rules.d/60-gpiochip4.rules
        fi
        
		echo "FPP - Disabling getty on onboard serial ttyAMA0"
		if [ "x${OSVER}" == "xdebian_13" ] || [ "x${OSVER}" == "xdebian_12" ]; then
			systemctl disable serial-getty@ttyAMA0.service
			sed -i -e "s/console=serial0,115200 //" ${BOOTDIR}/cmdline.txt
			sed -i -e "s/autologin pi/autologin ${FPPUSER}/" /etc/systemd/system/autologin@.service
            rm -f "/etc/systemd/system/getty@tty1.service.d/autologin.conf";
		fi
        swapoff /var/swap
        rm -f /var/swap
        rfkill unblock all
		;;
	#TODO
	'Armbian')
		echo "FPP - Armbian"
        if $isimage; then
            apt-get remove -y network-manager
            sed -i -e "s/overlays=/overlays=i2c0 spi-spidev /" /boot/armbianEnv.txt
            echo "param_spidev_spi_bus=0" >> /boot/armbianEnv.txt
            echo "param_spidev_max_freq=25000000" >> /boot/armbianEnv.txt
            
            sed -i -e "s/ENABLED=true/ENABLED=false/" /etc/default/armbian-ramlog
            
        fi
		;;
	'Ubuntu')
		echo "FPP - Ubuntu"
		;;
    'Mint')
		echo "FPP - Mint"
		;;
	'Debian')
		echo "FPP - Debian"
        if $isimage; then
            apt-get remove -y network-manager
            
            if $ISARMBIAN; then
                sed -i -e "s/overlays=/overlays=i2c0 spi-spidev /" /boot/armbianEnv.txt
                echo "param_spidev_spi_bus=0" >> /boot/armbianEnv.txt
                echo "param_spidev_max_freq=25000000" >> /boot/armbianEnv.txt
            
                sed -i -e "s/ENABLED=true/ENABLED=false/" /etc/default/armbian-ramlog
            fi
        fi
		;;
	*)
		echo "FPP - Unknown platform"
		;;
esac

#######################################
# Clone git repository
cd /opt
if $clone_fpp; then

    #######################################
    # Remove old /opt/fpp if it exists
    if [ -e "/opt/fpp" ]
    then
        echo "FPP - Removing old /opt/fpp"
        rm -rf /opt/fpp
    fi

    echo "FPP - Cloning git repository into /opt/fpp"
    git clone https://github.com/FalconChristmas/fpp fpp
    cd fpp
    git config pull.rebase true
fi
git config --global pull.rebase true
git config --global --add safe.directory /opt/fpp

# Add newfeatures remote for testing new features
cd /opt/fpp
if ! git remote get-url newfeatures > /dev/null 2>&1; then
    echo "FPP - Adding newfeatures remote"
    git remote add newfeatures https://github.com/FalconChristmas/fpp-new-feature-testing.git
fi

#######################################
# Build VLC
if $build_vlc; then
    echo "FPP - Building VLC"
    cd /opt/fpp/SD
    ./buildVLC.sh
    rm -rf /opt/vlc/
fi


#######################################
# Switch to desired code branch
echo "FPP - Switching git clone to ${FPPBRANCH} branch"
cd /opt/fpp
git checkout ${FPPBRANCH}
if [ "$FPPPLATFORM" == "Raspberry Pi" ]; then
    # force grabbing these upfront as the make -j4 may fail to fetch them due to locks
    # while initializing
    git submodule update --init external/RF24
    git submodule update --init external/rpi-rgb-led-matrix
    git submodule update --init external/rpi_ws281x
    git submodule update --init external/spixels
    cd /opt/fpp/src
    make gitsubmodules
    
    echo "FPP - Creating FPP DPI Overlays"
    cd /opt/fpp/capes/drivers/pi
    make -j ${CPUS}
    make install
    make clean
    cd ~
fi



#######################################
# Upgrade the config if needed
bash scripts/upgrade_config -notee


#######################################
PHPDIR="/etc/php/${ACTUAL_PHPVER}"

echo "FPP - Configuring PHP"
FILES="cli/php.ini apache2/php.ini fpm/php.ini"
for FILE in ${FILES}
do
    if [ -f ${PHPDIR}/${FILE} ]; then
        sed -i -e "s/^short_open_tag.*/short_open_tag = On/" ${PHPDIR}/${FILE}
        sed -i -e "s/max_execution_time.*/max_execution_time = 1000/" ${PHPDIR}/${FILE}
        sed -i -e "s/max_input_time.*/max_input_time = 900/" ${PHPDIR}/${FILE}
        sed -i -e "s/default_socket_timeout.*/default_socket_timeout = 900/" ${PHPDIR}/${FILE}
        sed -i -e "s/post_max_size.*/post_max_size = 1999M/" ${PHPDIR}/${FILE}
        sed -i -e "s/upload_max_filesize.*/upload_max_filesize = 1999M/" ${PHPDIR}/${FILE}
        sed -i -e "s/;upload_tmp_dir =.*/upload_tmp_dir = \/home\/${FPPUSER}\/media\/upload/" ${PHPDIR}/${FILE}
        sed -i -e "s/^;max_input_vars.*/max_input_vars = 5000/" ${PHPDIR}/${FILE}
        sed -i -e "s/^output_buffering.*/output_buffering = 1024/" ${PHPDIR}/${FILE}
    fi
done
FILES="fpm/pool.d/www.conf"
for FILE in ${FILES}
do
    if [ -f ${PHPDIR}/${FILE} ]; then
        sed -i -e "s/^listen.owner .*/listen.owner = ${FPPUSER}/" ${PHPDIR}/${FILE}
        sed -i -e "s/^listen.group .*/listen.group = ${FPPUSER}/" ${PHPDIR}/${FILE}
        sed -i -e "s/^user\ .*/user = ${FPPUSER}/" ${PHPDIR}/${FILE}
        sed -i -e "s/^group\ .*/group = ${FPPUSER}/" ${PHPDIR}/${FILE}
        sed -i -e "s/^pm.max_children.*/pm.max_children = 25/" ${PHPDIR}/${FILE}
        sed -i -e "s/^pm.min_spare_servers.*/pm.min_spare_servers = 3/" ${PHPDIR}/${FILE}
        sed -i -e "s/^pm.max_spare_servers.*/pm.max_spare_servers = 6/" ${PHPDIR}/${FILE}
        sed -i -e "s/^pm.start_servers.*/pm.start_servers = 3/" ${PHPDIR}/${FILE}
        sed -i -e "s/^;pm.max_requests.*/pm.max_requests = 300/" ${PHPDIR}/${FILE}
        sed -i -e "s+^;clear_env.*+clear_env = no+g" ${PHPDIR}/${FILE}
    fi
done

if $isimage; then
    #######################################
    echo "FPP - Copying rsync daemon config files into place"
    sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" -e "s#FPPUSER#${FPPUSER}#g" < ${FPPDIR}/etc/rsync > /etc/default/rsync
    sed -e "s#FPPDIR#${FPPDIR}#g" -e "s#FPPHOME#${FPPHOME}#g" -e "s#FPPUSER#${FPPUSER}#g" < ${FPPDIR}/etc/rsyncd.conf > /etc/rsyncd.conf
    systemctl disable rsync
fi

#######################################
# Add the ${FPPUSER} user and group memberships
echo "FPP - Adding ${FPPUSER} user"
addgroup --gid 500 ${FPPUSER}
adduser --uid 500 --home ${FPPHOME} --shell /bin/bash --ingroup ${FPPUSER} --gecos "Falcon Player" --disabled-password ${FPPUSER}
adduser ${FPPUSER} adm
adduser ${FPPUSER} sudo
if getent group gpio > /dev/null; then
    adduser ${FPPUSER} gpio
fi

if getent group i2c > /dev/null; then
    adduser ${FPPUSER} i2c
fi
if getent group spi > /dev/null; then
    adduser ${FPPUSER} spi
fi
adduser ${FPPUSER} video
adduser ${FPPUSER} audio
adduser ${FPPUSER} dialout
# FIXME, use ${FPPUSER} here instead of hardcoding
sed -i -e 's/^fpp:.*:/fpp:\$y\$j9T\$w6Yvb\/z1wtJh6TvI1zs8u\.\$ZEhs\/1W7JZlCrVCiVvsjarXzf\.3kY7GFHEpWIOQMPN6:20285:0:99999:7:::/' /etc/shadow

if $isimage; then
    echo "FPP - Disabling any stock 'debian' user, use the '${FPPUSER}' user instead"
    sed -i -e "s/^debian:.*/debian:*:16372:0:99999:7:::/" /etc/shadow
    
    #######################################
    echo "FPP - Fixing empty root passwd"
    sed -i -e 's/root::/root:*:/' /etc/shadow

    echo "FPP - Setting up ssh to disallow root login"
    sed -i -e "s/#PermitRootLogin .*/PermitRootLogin prohibit-password/" /etc/ssh/sshd_config
    sed -i -e "s/#\?Banner .*/Banner \/etc\/issue.net/g" /etc/ssh/sshd_config

    echo "FPP - Cleaning up /root/.cpanm to save space on the SD image"
    rm -rf /root/.cpanm
    
    echo "FPP - Enabling legacy WiFi drivers"
    sed -i -e "s@^ExecStart.*@ExecStart=/sbin/wpa_supplicant -c/etc/wpa_supplicant/wpa_supplicant-%I.conf -Dnl80211,wext -i%I@" "/lib/systemd/system/wpa_supplicant@.service"
fi


#######################################
echo "FPP - Populating ${FPPHOME}"
mkdir ${FPPHOME}/.ssh
chown ${FPPUSER}:${FPPUSER} ${FPPHOME}/.ssh
chmod 700 ${FPPHOME}/.ssh

mkdir ${FPPHOME}/media
chown ${FPPUSER}:${FPPUSER} ${FPPHOME}/media
chmod 775 ${FPPHOME}/media

cat > ${FPPHOME}/.vimrc <<-EOF
set tabstop=4
set shiftwidth=4
set expandtab
set autoindent
set ignorecase
set mouse=r
syntax on
EOF

chmod 644 ${FPPHOME}/.vimrc
chown ${FPPUSER}:${FPPUSER} ${FPPHOME}/.vimrc

echo >> ${FPPHOME}/.bashrc
echo ". /opt/fpp/scripts/common" >> ${FPPHOME}/.bashrc
echo >> ${FPPHOME}/.bashrc

mkdir ${FPPHOME}/media/logs
chown fpp:fpp ${FPPHOME}/media/logs

#######################################
# Configure log rotation
echo "FPP - Configuring log rotation"
cp /opt/fpp/etc/logrotate.d/* /etc/logrotate.d/
sed -i -e "s/#compress/compress/" /etc/logrotate.conf
sed -i -e "s/rotate .*/rotate 2/" /etc/logrotate.conf
sed -i -e "s/weekly/daily/" /etc/logrotate.d/rsyslog

#######################################
# Disable duplicate logging to save on disk space 
sed -i '/auth,authpriv\.\*/s/^/# /' /etc/rsyslog.conf
sed -i '/cron\.\*/s/^/# /' /etc/rsyslog.conf
sed -i '/kern\.\*/s/^/# /' /etc/rsyslog.conf
sed -i '/mail\.\*/s/^/# /' /etc/rsyslog.conf
sed -i '/user\.\*/s/^/# /' /etc/rsyslog.conf

#######################################
# Configure ccache
echo "FPP - Configuring ccache"
mkdir -p /root/.ccache
ccache -M 500M
ccache --set-config=temporary_dir=/tmp
ccache --set-config=sloppiness=pch_defines,time_macros
ccache --set-config=hard_link=true
ccache --set-config=pch_external_checksum=true
mkdir -p /home/fpp/.config/ccache
cp /root/.ccache/ccache.conf /home/fpp/.config/ccache
chown -R fpp:fpp /home/fpp/.config

if $isimage; then
    #######################################
    echo "FPP - Configuring FTP server"
    sed -i -e "s/.*anonymous_enable.*/anonymous_enable=NO/" /etc/vsftpd.conf
    sed -i -e "s/.*local_enable.*/local_enable=YES/" /etc/vsftpd.conf
    sed -i -e "s/.*write_enable.*/write_enable=YES/" /etc/vsftpd.conf
    systemctl disable vsftpd

    #######################################
    cp ${FPPHOME}/.vimrc /root/.vimrc

    #######################################
    echo "FPP - Configuring Samba"
    cat <<-EOF >> /etc/samba/smb.conf

[nobody]
  browseable = no

[FPP]
  comment = FPP Media Share
  path = ${FPPHOME}/media
  veto files = /._*/.DS_Store/
  delete veto files = yes
  writeable = Yes
  only guest = Yes
  create mask = 0777
  directory mask = 0777
  browseable = Yes
  public = yes
  force user = ${FPPUSER}

EOF

    systemctl disable smbd
    systemctl disable nmbd

fi

#######################################
# Fix sudoers to not require password
echo "FPP - Giving ${FPPUSER} user sudo"
echo "${FPPUSER} ALL=(ALL:ALL) NOPASSWD: ALL" >> /etc/sudoers


if $isimage; then
    #######################################
    # Setup mail
    echo "FPP - Adding missing exim4 & ntpsec log directory"
    mkdir /var/log/ntpsec
    chown ntpsec:ntpsec /var/log/ntpsec
    mkdir /var/log/exim4
    chown Debian-exim /var/log/exim4
    chgrp Debian-exim /var/log/exim4

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
    ARCH=$(uname -m)
    if [ "$ARCH" != "x86_64" ]; then
        echo "FPP - Configuring tmpfs filesystems"
        sed -i 's|tmpfs\s*/tmp\s*tmpfs.*||g' /etc/fstab
        echo "#####################################" >> /etc/fstab
        echo "tmpfs         /tmp        tmpfs   nodev,nosuid,size=75M 0 0" >> /etc/fstab
        if [ "$FPPPLATFORM" != "BeagleBone 64" ]; then
            echo "tmpfs         /var/tmp    tmpfs   nodev,nosuid,size=50M 0 0" >> /etc/fstab
        fi
        echo "#####################################" >> /etc/fstab
    fi

    COMMENTED=""
    SDA1=$(lsblk -l | grep sda1 | awk '{print $7}')
    if [ -n ${SDA1} ]
    then
        COMMENTED="#"
    fi
    echo "${COMMENTED}/dev/sda1     ${FPPHOME}/media  auto    defaults,nonempty,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500  0  0" >> /etc/fstab
    echo "#####################################" >> /etc/fstab

    #######################################
    #  Prefer IPv4
    echo "FPP - Prefer IPv4"
    echo "precedence ::ffff:0:0/96  100" >>  /etc/gai.conf

    # need to keep extra memory to process network packets
    echo "vm.min_free_kbytes=16384" >> /etc/sysctl.conf
fi

#######################################
echo "FPP - Configuring Apache webserver"

# Environment variables
sed -i -e "s/APACHE_RUN_USER=.*/APACHE_RUN_USER=${FPPUSER}/" /etc/apache2/envvars
sed -i -e "s/APACHE_RUN_GROUP=.*/APACHE_RUN_GROUP=${FPPUSER}/" /etc/apache2/envvars
sed -i -e "s#APACHE_LOG_DIR=.*#APACHE_LOG_DIR=${FPPHOME}/media/logs#" /etc/apache2/envvars
sed -i -e "s/Listen 8080.*/Listen 80/" /etc/apache2/ports.conf

#Copy FPP Defined Apache configs
cat /opt/fpp/etc/apache2.site > /etc/apache2/sites-enabled/000-default.conf
cat /opt/fpp/etc/apache2.status > /etc/apache2/mods-enabled/status.conf

# Enable Apache modules
a2dismod php${ACTUAL_PHPVER}
a2dismod mpm_prefork
a2enmod mpm_event
a2enmod http2
a2enmod cgi
a2enmod rewrite
a2enmod expires
a2enmod proxy
a2enmod proxy_http
a2enmod proxy_http2
a2enmod proxy_html
a2enmod headers
a2enmod proxy_fcgi setenvif
a2enconf php${ACTUAL_PHPVER}-fpm

# Fix name of Apache default error log so it gets rotated by our logrotate config
sed -i -e "s/error\.log/apache2-base-error.log/" /etc/apache2/apache2.conf

# Disable default access logs
rm /etc/apache2/conf-enabled/other-vhosts-access-log.conf

case "${OSVER}" in
	debian_13 |  debian_12 | ubuntu_22.04 | ubuntu_22.10 | linuxmint_21)
		systemctl enable apache2.service
		;;
esac


#######################################
echo "FPP - Configuring NTP Daemon"

# Clear all existing servers and pools and set default pool to be falconplayer NTP Pool
sed -i '/^server.*/d' /etc/ntpsec/ntp.conf 
sed -i '/^pool.*/d' /etc/ntpsec/ntp.conf 
sed -i '$s/$/\npool falconplayer.pool.ntp.org iburst minpoll 8 maxpoll 12 prefer/' /etc/ntpsec/ntp.conf


if [ "x${FPPPLATFORM}" = "xBeagleBone 64" ]; then
    #Set colored prompt
    sed -i -e "s/#force_color_prompt=yes/force_color_prompt=yes/" /home/fpp/.bashrc
    # remove the udev rules that create the SoftAp interface on the bbbw and bbggw
    rm -f /etc/udev/rules.d/*SoftAp*
    
    echo 'GOVERNOR="schedutil"' > /etc/default/cpufrequtils
    
    echo "USB_UMTPRD_DISABLED=yes" >> /etc/default/bb-boot
fi
if [ "x${FPPPLATFORM}" = "xBeagleBone Black" ]; then
    #######################################
    systemctl disable dev-hugepages.mount
    
    # CPU frequency scaling is disabled in our kernel, no need for the service to run
    systemctl disable cpufrequtils
    systemctl disable loadcpufreq.service
    
    # sysconf requires a vfat partition which the BBB images currently don't have
    systemctl disable bbbio-set-sysconf
    echo "USB_UMTPRD_DISABLED=yes" >> /etc/default/bb-boot

    if [ ! -f "/opt/source/bb.org-overlays/Makefile" ]; then
        mkdir -p /opt/source
        cd /opt/source
        git clone https://github.com/beagleboard/bb.org-overlays
    fi
    
    cd /opt/fpp/capes/drivers/bbb
    make -j ${CPUS}
    make install
    make clean
        
    #Set colored prompt
    sed -i -e "s/#force_color_prompt=yes/force_color_prompt=yes/" /home/fpp/.bashrc
    
    #adjust a bunch of settings in /boot/uEnv.txt
    sed -i -e "s+^#enable_uboot_cape_universal=\(.*\)+enable_uboot_cape_universal=1+g"  /boot/uEnv.txt
    sed -i -e "s+^#disable_uboot_overlay_video=\(.*\)+disable_uboot_overlay_video=1+g"  /boot/uEnv.txt
    sed -i -e "s+#uboot_overlay_addr0=\(.*\)+uboot_overlay_addr0=/lib/firmware/fpp-base-overlay.dtb+g"  /boot/uEnv.txt
    sed -i -e "s+#uboot_overlay_addr1=\(.*\)+uboot_overlay_addr1=/lib/firmware/fpp-cape-overlay.dtb+g"  /boot/uEnv.txt
    sed -i -e "s+ quiet+ quiet rootwait+g"  /boot/uEnv.txt
    sed -i -e "s+ net.ifnames=.+ +g"  /boot/uEnv.txt
    sed -i -e "s+^uboot_overlay_pru=+#uboot_overlay_pru=+g"  /boot/uEnv.txt
    echo "bootdelay=0" >> /boot/uEnv.txt
    echo "#cmdline=init=/opt/fpp/SD/BBB-AutoFlash.sh" >> /boot/uEnv.txt

    # remove the udev rules that create the SoftAp interface on the bbbw and bbggw
    rm -f /etc/udev/rules.d/*SoftAp*
    
    #cleanout some DRI files that are not needed at all on Beagle hardware
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/r?00*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/rad*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/nouveau*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/rockchip*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/st*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/ili*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/imx*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/exy*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/etn*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/hx*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/kirin*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/rcar*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/mediatek*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/tegra*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/komeda*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/v3d*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/vc4*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/virtio*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/zink*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/lima*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/panfrost*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/armada*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/mi0283qt*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/pl111*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/msm*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/mcde*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/meson*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/ingenic*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/mali*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/mxs*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/rep*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/sun*
    rm -rf /usr/lib/arm-linux-gnueabihf/dri/vmw*
fi
if [ "x${FPPPLATFORM}" = "xBeagleBone 64" ]; then    
    cd /opt/fpp/capes/drivers/bb64
    make -j ${CPUS}
    make install
    make clean
    cp /boot/firmware/extlinux/extlinux.conf /boot/firmware/extlinux/extlinux.conf.orig
    cp extlinux/extlinux.conf /boot/firmware/extlinux/extlinux.conf
    cd ~
        
    #Set colored prompt
    sed -i -e "s/#force_color_prompt=yes/force_color_prompt=yes/" /home/fpp/.bashrc
    
    systemctl disable console-setup.service
fi

if $isimage; then
    rm -rf /usr/share/doc/*
    apt-get clean
fi


#######################################
echo "FPP - Configuring FPP startup"
cp /opt/fpp/etc/systemd/*.service /lib/systemd/system/
cp /opt/fpp/etc/avahi/* /etc/avahi/services
if $isimage; then
    mkdir -p /etc/networkd-dispatcher/initialized.d
    cp -a /opt/fpp/etc/networkd-dispatcher/* /etc/networkd-dispatcher
fi

systemctl disable mosquitto
systemctl daemon-reload
systemctl enable fppinit.service
systemctl enable fpprtc.service
systemctl enable fppoled.service
systemctl enable fppd.service
systemctl enable fpp_postnetwork.service
systemctl enable fpp-install-kiosk.service
systemctl enable fpp-reboot.service

if $isimage; then
    cp /opt/fpp/etc/update-RTC /etc/cron.daily

    # Make sure the journal is large enough to store the full boot logs
    # but not get too large which starts slowing down journalling (and thus boot)
    if [ -f /etc/systemd/journald.conf ]; then
        sed -i -e "s/^.*SystemMaxUse.*/SystemMaxUse=96M/g" /etc/systemd/journald.conf
    fi
    
    rm -f /etc/systemd/network/*eth*
    rm -f /etc/systemd/network/*wlan*
    cp /opt/fpp/etc/systemd/network/* /etc/systemd/network
fi


echo "FPP - Compiling binaries"
cd /opt/fpp/src/
make clean ; make -j ${CPUS} optimized


######################################
if [ "$FPPPLATFORM" == "Raspberry Pi" -o "$FPPPLATFORM" == "BeagleBone Black" -o "$FPPPLATFORM" == "BeagleBone 64" ]; then
    # replace entry already there
    sed -i 's/^DAEMON_CONF.*/DAEMON_CONF="\/etc\/hostapd\/hostapd.conf"/g' /etc/default/hostapd
    if ! grep -q etc/hostapd/hostapd.conf "/etc/default/hostapd"; then
        # not there, append after the commented out default line
        sed -i 's/^#DAEMON_CONF\(.*\)/#DAEMON_CONF\1\nDAEMON_CONF="\/etc\/hostapd\/hostapd.conf"/g' /etc/default/hostapd
    fi
    if ! grep -q etc/hostapd/hostapd.conf "/etc/default/hostapd"; then
        # default line not there, just append to end of file
        echo "DAEMON_CONF=\"/etc/hostapd/hostapd.conf\"" >> /etc/default/hostapd
    fi
        
    systemctl disable zramswap
    echo "ALGO=zstd" >> /etc/default/zramswap
    echo "PRIORITY=100" >> /etc/default/zramswap
    if [ "$FPPPLATFORM" == "BeagleBone 64" ]; then
        echo "SIZE=125" >> /etc/default/zramswap
        echo "vm.swappiness=100" >> /etc/sysctl.d/10-swap.conf
        echo "vm.vfs_cache_pressure=100" >> /etc/sysctl.d/10-swap.conf
        echo "vm.dirty_background_ratio=1" >> /etc/sysctl.d/10-swap.conf
        echo "vm.dirty_ratio=50" >> /etc/sysctl.d/10-swap.conf
    else
        echo "SIZE=75" >> /etc/default/zramswap
        echo "vm.swappiness=1" >> /etc/sysctl.d/10-swap.conf
        echo "vm.vfs_cache_pressure=50" >> /etc/sysctl.d/10-swap.conf
    fi
    
    if $isimage; then
        # make sure the existing users are completely removed
        rm -rf /home/pi
        rm -rf /home/debian
    fi
fi
if $isimage; then
    systemctl disable dnsmasq
    systemctl unmask hostapd
    systemctl disable hostapd
fi
if [ "$FPPPLATFORM" == "BeagleBone Black" ]; then
    # Bootloader on recent bullseye images does NOT boot on Beagles, use a version we
    # know works
    /opt/fpp/bin.bbb/bootloader/install.sh
fi
if $isimage; then
    rm -f /etc/resolv.conf
    ln -s /run/systemd/resolve/resolv.conf /etc/resolv.conf
fi

# generate apache csp file
mkdir -p /home/fpp/media/config/
/opt/fpp/scripts/ManageApacheContentPolicy.sh regenerate

ENDTIME=$(date)

echo "========================================================="
echo "FPP Install Complete."
echo "Started : ${STARTTIME}"
echo "Finished: ${ENDTIME}"
echo "========================================================="
echo "You can reboot the system by changing to the '${FPPUSER} user with the"
echo "password 'falcon' and running the shutdown command."
echo ""
echo "su - ${FPPUSER}"
echo "sudo shutdown -r now"
echo ""
echo "NOTE: If you are prepping this as an image for release,"
echo "remove the SSH keys before shutting down so they will be"
echo "rebuilt during the next boot."
echo ""
echo "su - ${FPPUSER}"
echo "sudo rm -rf /etc/ssh/ssh_host*key*"
echo "sudo shutdown -r now"
echo "========================================================="
echo ""

cp /root/FPP_Install.* ${FPPHOME}/
chown fpp:fpp ${FPPHOME}/FPP_Install.*
