#!/bin/bash

set -e

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
#       main goal is to prep these release images.  It may be used for other
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
# NOTE: FPP Development targets Debian 13 (Trixie) and Ubuntu 24.04. Earlier
#       distributions are not supported. Supported hosts:
#         - Raspberry Pi OS (Trixie, armhf/arm64)
#         - BeagleBone (Debian 13 armhf/arm64, rcn-ee base images)
#         - Debian 13 / Ubuntu 24.04 on x86_64 or arm64 generic hardware
#         - Armbian built on a Debian 13 or Ubuntu 24.04 base
#         - Fedora 40+ (best-effort; primarily dev-host support)
#
#############################################################################
FPPBRANCH=${FPPBRANCH:-"master"}
# FPPIMAGEVER is the OS image date stamp written into /etc/fpp/rfs_version
# and shown in /etc/issue. Override via env (build-image-pi.sh passes the
# user-supplied --os-version so the .img / .fppos filenames match what's
# baked into the image itself).
FPPIMAGEVER=${FPPIMAGEVER:-"2026-04"}
FPPCFGVER="104"
FPPPLATFORM="UNKNOWN"
FPPDIR=/opt/fpp
FPPUSER=fpp
FPPHOME=/home/${FPPUSER}
OSVER="UNKNOWN"

# Make sure the sbin directories are on the path as we will
# need the adduser/addgroup/ldconfig/a2enmod/etc... commands
PATH=$PATH:/usr/sbin:/sbin

# Under docker / chroot (systemd isn't PID 1), runtime systemctl commands
# (start/stop/restart/reload/is-active/daemon-reload/...) fail because there
# is no running systemd to talk to. But unit-symlink operations (enable,
# disable, mask, unmask, preset, set-default) work perfectly well -- they
# just manipulate /etc/systemd/system/*.wants/ symlinks, which is exactly
# what we need during image builds.
#
# A blanket "neutralize all systemctl" override silently drops every
# `systemctl enable fppd.service` in the script and produces an image where
# none of FPP's services are registered with systemd. That's how broken Pi
# images shipped before this fix.
#
# Selective wrapper: pass through unit-management commands (with --no-reload
# so they don't try to talk to a running daemon); no-op everything else.
#
# disable / unmask are "remove from active state" -- idempotent by intent;
# many of FPP's calls target units that may or may not exist depending on
# platform (BB units don't exist on Pi, gdm/lightdm don't exist on Lite
# images). Failures from these are noise we want to swallow so set -e
# doesn't kill the install on the first absent unit.
#
# enable / mask / preset / etc. are "add to active state" -- failures from
# these mean we got something we wanted wrong; surface them.
if [ ! -d /run/systemd/system ]; then
    _real_systemctl="$(command -v systemctl)"
    systemctl() {
        case "$1" in
            disable|unmask)
                "$_real_systemctl" --no-reload --no-warn "$@" 2>/dev/null || true
                ;;
            enable|mask|preset|preset-all|reenable|set-default|add-wants|add-requires)
                "$_real_systemctl" --no-reload "$@"
                ;;
            *)
                echo "skipping systemctl $*"
                return 0
                ;;
        esac
    }
    export -f systemctl
fi

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
	# www.usno.navy.mil is not pingable, so check github.com instead.
	# Inline the ping into the if so set -e doesn't abort on ping failure
	# (ICMP often unavailable in chroots / qemu-user contexts).
	if ping -q -c 1 github.com > /dev/null 2>&1
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
if [ "x${OSID}" = "xraspbian" ] || [ -f /etc/rpi-issue ]
then
	FPPPLATFORM="Raspberry Pi"
	OSVER="debian_${VERSION_ID}"
    isimage=true
    desktop=false
elif [ -f /etc/bbb.io/templates/bb-boot ] \
        || { [ -f /boot/firmware/ID.txt ] && grep -qi "beagleboard" /boot/firmware/ID.txt 2>/dev/null; } \
        || [ -e "/sys/class/leds/beaglebone:green:usr1" ]
then
    # BeagleBoard detection, in priority order:
    #   1. /etc/bbb.io/templates/bb-boot -- present on all modern Beagle rootfs
    #      builds; visible inside chroot (rootfs is mounted during image builds).
    #   2. /boot/firmware/ID.txt -- stamped by the rcn-ee installer on BB64 and
    #      recent BBB builds; contents begin with "BeagleBoard.org ...".
    #   3. /sys/class/leds/beaglebone:green:usr1 -- only works on booted real
    #      hardware; kept as legacy fallback.
    if [ "$(uname -m)" = "aarch64" ]; then
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
elif [ "x${OSID}" = "xfedora" ]
then
    FPPPLATFORM="Fedora"
else
	FPPPLATFORM="UNKNOWN"
fi

# Build-time override: BeagleBone autodetection relies on /sys/class/leds
# entries that don't exist in a chroot, so image-build scripts can force
# the platform string explicitly. Honored after autodetect so it always wins.
if [ -n "${FPPPLATFORM_OVERRIDE:-}" ]; then
    FPPPLATFORM="${FPPPLATFORM_OVERRIDE}"
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
        --yes|-y)
            FPP_INSTALL_YES=1
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

if [ "x${FPP_INSTALL_YES}" = "x1" ]; then
    echo "Non-interactive mode (FPP_INSTALL_YES=1), proceeding."
else
    echo -n "Do you wish to proceed? [N/y] "
    read ANSWER
    if [ "x${ANSWER}" != "xY" -a "x${ANSWER}" != "xy" ]
    then
        echo
        echo "Install cancelled."
        echo
        exit
    fi
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
    rm -f /etc/fpp/arch
    rm -f /etc/fpp/rfs_version
    rm -f /etc/fpp/desktop
fi

#######################################
# Create /etc/fpp directory and contents
echo "FPP - Creating /etc/fpp and contents"
mkdir /etc/fpp
echo "${FPPPLATFORM}" > /etc/fpp/platform
# Architecture marker (e.g. armhf / arm64 / x86_64). Used by upgradeOS to
# refuse cross-arch .fppos applies — critical because FPPPLATFORM alone
# cannot distinguish 32-bit vs 64-bit Pi (both write "Raspberry Pi").
echo "$(dpkg --print-architecture 2>/dev/null || uname -m)" > /etc/fpp/arch
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


#############################################################################
# install_base_packages
# Top-level apt phase: remove unwanted stock packages, update apt, select PHP
# version, install the full FPP dependency set, and do the one-off Pi USB-boot
# fstab/cmdline plumbing that needs to run once-per-image.
# Reads globals: FPPPLATFORM, OSVER, desktop, isimage, skip_apt_install, build_vlc
# Sets globals:  ACTUAL_PHPVER, PHPVER, BOOTDIR (on Pi)
#############################################################################
install_base_packages() {
    case "${OSVER}" in
        debian_13 | ubuntu_24.*) ;;
        *)
            echo "ERROR: unsupported OS for install_base_packages: ${OSVER}" >&2
            echo "Supported: Debian 13 (Trixie), Ubuntu 24.04+" >&2
            exit 1
            ;;
    esac

        # Stop unattended-upgrades as it can hold a lock on the apt repository
        systemctl stop unattended-upgrades

        #remove a bunch of packages that aren't neeeded, free's up space
        PACKAGE_REMOVE="nginx nginx-full nginx-common  triggerhappy pocketsphinx-en-us guile-2.2-libs \
            gfortran glib-networking libxmuu1 xauth network-manager dhcpcd5 fake-hwclock ifupdown isc-dhcp-client isc-dhcp-common openresolv \
            unattended-upgrades packagekit iwd"
        if [ "$FPPPLATFORM" == "BeagleBone 64" -o "$FPPPLATFORM" == "BeagleBone Black" ]; then
            PACKAGE_REMOVE="$PACKAGE_REMOVE nodejs mender-client bb-code-server"
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
  
        # PHP version selection. Each supported OS ships with a recent PHP
        # in the stock repo -- no PPA, no backports.
        if [ "${OSVER}" == "debian_13" ]; then
            ACTUAL_PHPVER="8.4"
            PHPVER="8.4"
        elif [[ "${OSVER}" == ubuntu_24.* ]]; then
            ACTUAL_PHPVER="8.3"
            PHPVER="8.3"
        else
            echo "ERROR: unsupported OS for PHP selection: ${OSVER}" >&2
            exit 1
        fi

        #########################################
        # WARNING
        # ---------------------------------------
        # If you modify this file, be sure to update Docker/Dockerfile
        #########################################

        PACKAGE_LIST="alsa-utils arping avahi-daemon avahi-utils locales nano net-tools distcc \
                      apache2 apache2-bin apache2-data apache2-utils libavahi-client-dev util-linux-extra \
                      bc bash-completion btrfs-progs exfat-fuse lsof ethtool curl zip unzip bzip2 wireless-tools dos2unix \
                      fbi fbset file flite ca-certificates lshw gettext wget iproute2 fswatch \
                      build-essential ffmpeg gcc g++ gdb vim vim-common bison flex device-tree-compiler dh-autoreconf \
                      git hdparm i2c-tools jq less sysstat tcpdump time usbutils usb-modeswitch \
                      samba rsync sudo shellinabox dnsmasq hostapd vsftpd sqlite3 at haveged samba samba-common-bin \
                      mp3info exim4 mailutils dhcp-helper parprouted bridge-utils libiio-utils libhidapi-dev \
                      php${PHPVER} php${PHPVER}-cli php${PHPVER}-fpm php${PHPVER}-common php${PHPVER}-curl php-pear \
                      php${PHPVER}-bcmath php${PHPVER}-sqlite3 php${PHPVER}-zip php${PHPVER}-xml ccache \
                      libavcodec-dev libavformat-dev libswresample-dev libswscale-dev libavdevice-dev libavfilter-dev libtag1-dev \
                      vorbis-tools libgraphicsmagick++1-dev graphicsmagick-libmagick-dev-compat libdrogon-dev \
                      gettext apt-utils x265 libtheora-dev libvorbis-dev libx265-dev iputils-ping mp3gain clang-format \
                      libmosquitto-dev mosquitto-clients mosquitto libzstd-dev lzma zstd gpiod libgpiod-dev libjsoncpp-dev libcurl4-openssl-dev \
                      fonts-freefont-ttf flex bison pkg-config libasound2-dev libsdl2-dev mesa-common-dev qrencode libusb-1.0-0-dev \
                      pipewire-alsa pipewire-jack pipewire-audio-client-libraries libpipewire-0.3-dev pulseaudio-utils linuxptp gstreamer1.0-tools \
                      gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-pipewire \
                      gstreamer1.0-libav libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev wireplumber pipewire-bin pipewire \
                      flex bison pkg-config libasound2-dev python3-setuptools libssl-dev libtool bsdextrautils iw rsyslog tzdata libsystemd-dev"

        if [ "$FPPPLATFORM" == "Raspberry Pi" -o "$FPPPLATFORM" == "BeagleBone Black"  -o "$FPPPLATFORM" == "BeagleBone 64" ]; then
            PACKAGE_LIST="$PACKAGE_LIST firmware-realtek firmware-atheros firmware-ralink firmware-brcm80211 firmware-iwlwifi firmware-libertas firmware-zd1211 firmware-ti-connectivity zram-tools"
            if [ "$FPPPLATFORM" == "Raspberry Pi" ]; then
                PACKAGE_LIST="$PACKAGE_LIST libva-dev smartmontools edid-decode kms++-utils"
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
            # Debian 13 breaks vlc into several plugin sub-packages; Ubuntu
            # 24.04 keeps them bundled in the main vlc package.
            if [ "${OSVER}" == "debian_13" ]; then
                PACKAGE_LIST="$PACKAGE_LIST vlc-plugin-pipewire vlc-plugin-base vlc-plugin-video-output"
            fi
        fi
        PACKAGE_LIST="$PACKAGE_LIST ntpsec pipewire"
        
        if $skip_apt_install; then
            echo "skipping apt install because skpt_apt_install == $skip_apt_install"
            PACKAGE_LIST=""
        else
            echo "--------------------------"
            echo "INSTALLING $PACKAGE_LIST"
            echo "--------------------------"
            apt-get -y -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" install ${PACKAGE_LIST}
        fi
        
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
			# beagle-flasher: present on current rcn-ee BB images and would
			# otherwise re-flash the eMMC on shutdown. Disable it so FPP
			# images don't surprise users by overwriting their installation.
			systemctl disable beagle-flasher-init-shutdown.service

			# Mask systemd-firstboot so it doesn't prompt for a username on
			# first boot. We've already created the fpp user; the prompt is
			# noise (and confuses non-headless users on Pi where the prompt
			# appears on the HDMI console). Mask, not disable: -firstboot is
			# gated by a marker file too, but masking guarantees no run.
			systemctl mask systemd-firstboot.service || true
		fi

        if $isimage; then
            echo "FPP - Disabling GUI"
            # gdm3 (current GNOME), lightdm (Pi OS Desktop / Mint), and the
            # generic display-manager alias cover the desktop-DM cases we
            # might encounter when --img is run on a desktop install. The
            # legacy gdm/wdm/xdm names are gone from supported releases.
            systemctl disable gdm3
            systemctl disable lightdm
            systemctl disable display-manager.service
            
            echo "FPP - Disabling dhcp-helper and hostapd from automatically starting"
            systemctl disable dhcp-helper
            systemctl disable hostapd
            
            systemctl disable distcc
            
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

                # make sure the label on p1 is "boot" and p2 is rootfs.
                # These device paths don't exist in a chroot image build, and
                # even on real hardware the labels are typically already set;
                # tolerate failure so the install doesn't abort under set -e.
                fatlabel /dev/mmcblk0p1 boot 2>/dev/null || true
                e2label /dev/mmcblk0p2 rootfs 2>/dev/null || true

                # Debian 12+ Pi images always mount firmware at /boot/firmware.
                BOOTDIR="/boot/firmware"

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

}

install_base_packages


#######################################
# Platform-specific config
#############################################################################
# Platform-specific setup functions.
# Each one is invoked from the case statement below based on FPPPLATFORM.
# They rely on globals set earlier ($isimage, $FPPUSER, $OSVER, $skip_apt_install)
# and may set $BOOTDIR as a side effect.
#############################################################################

# apt_purge_installed <pkg> [<pkg>...]
# Purge only packages that are actually installed. Needed under set -e since
# apt-get purge on a missing package would otherwise abort the script.
apt_purge_installed() {
    local installed="" pkg
    for pkg in "$@"; do
        if dpkg -s "$pkg" >/dev/null 2>&1; then
            installed="$installed $pkg"
        fi
    done
    if [ -n "$installed" ]; then
        apt-get -y purge $installed
    fi
}

setup_platform_beaglebone_black() {
    # Blacklist gyro/barometer on SanCloud enhanced (conflicts with cape pins).
    cat > /etc/modprobe.d/blacklist-gyro.conf <<'EOF'
blacklist st_pressure_spi
blacklist st_pressure_i2c
blacklist st_pressure
blacklist inv_mpu6050_i2c
blacklist st_sensors_spi
blacklist st_sensors_i2c
blacklist inv_mpu6050
blacklist st_sensors
EOF
    echo "blacklist spidev" > /etc/modprobe.d/blacklist-spidev.conf

    # Blacklist bluetooth on BBG Gateway / BBGW / BBBW -- kernel instability,
    # and we don't use it.
    cat > /etc/modprobe.d/blacklist-bluetooth.conf <<'EOF'
blacklist btusb
blacklist bluetooth
blacklist hci_uart
blacklist bnep
EOF

    rm -f /etc/modules-load.d/network.conf

    systemctl disable keyboard-setup
    systemctl disable unattended-upgrades
    systemctl disable resize_filesystem
    systemctl disable console-setup
}

setup_platform_beaglebone_64() {
    systemctl disable keyboard-setup
    systemctl disable unattended-upgrades
    systemctl disable mender-client
    systemctl disable resize_filesystem
    systemctl disable console-setup
    systemctl disable samba-ad-dc

    echo "FPP - Adding required modules to modules-load to speed up boot"
    cat >> /etc/modules-load.d/modules.conf <<'EOF'
snd_pcm
snd_timer
snd
soundcore
irq_pruss_intc
pru_rproc
cpufreq_dt
rti_wdt
at24
ad7291
pruss
omap_mailbox
loop
efi_pstore
dm_mod
ip_tables
EOF
}

setup_platform_raspberry_pi() {
    echo "FPP - Updating firmware for Raspberry Pi install"
    apt-get dist-upgrade -y

    echo "FPP - Installing Pi-specific packages"
    apt-get -y install raspi-config

    if $isimage; then
        # Debian 12+ Pi images always mount firmware at /boot/firmware.
        BOOTDIR="/boot/firmware"

        echo "FPP - Disabling stock users (pi, odroid, debian), use '${FPPUSER}' instead"
        sed -i -e "s/^pi:.*/pi:*:16372:0:99999:7:::/"         /etc/shadow
        sed -i -e "s/^odroid:.*/odroid:*:16372:0:99999:7:::/" /etc/shadow
        sed -i -e "s/^debian:.*/debian:*:16372:0:99999:7:::/" /etc/shadow

        echo "FPP - Tweaking ${BOOTDIR}/config.txt"
        # Camera auto-detect off (we don't use it; saves a bit of boot time).
        sed -i -e "s/camera_auto_detect/#camera_auto_detect/"  ${BOOTDIR}/config.txt
        # NOTE: leave hdmi_force_hotplug alone. With the 6.18 vc4 KMS driver,
        # disabling it lets the GPU drop the HDMI signal entirely when the
        # kernel's hot-plug detection misses (which it does often enough on
        # a fresh boot), leaving the monitor in powersave with no console
        # visible. Old FPP images used to comment this out -- don't.

        echo "FPP - Adding required modules to modules-load to speed up boot"
        cat >> /etc/modules-load.d/modules.conf <<'EOF'
i2c_dev
spidev
at24
lm75
snd_bcm2835
bcm2835_codec
snd_usb_audio
EOF

        # Append FPP-specific device-tree and hardware config to config.txt
        cat >> ${BOOTDIR}/config.txt <<'EOF'
[all]

# Enable SPI in device tree
dtparam=spi=on

# Enable PCIe for NVME storage (Gen2 is the Pi 5's certified speed; Gen3
# is an overclock that some NVMe HATs/drives fail to train reliably at)
dtparam=pciex1_gen=2

# Enable Cape Specific Overlays
dtoverlay=fpp-cape-overlay

# Enable I2C in device tree
dtparam=i2c_arm=on,i2c_arm_baudrate=400000

# Setting kernel scaling framebuffer method
scaling_kernel=8

# Enable audio
dtparam=audio=on

# Allow more current through USB
max_usb_current=1

# Setup UART clock to allow DMX output
init_uart_clock=16000000

# Swap Pi 3 and Zero W UARTs with BT
dtoverlay=miniuart-bt

# Model Specific configuration
# GPU memory set to 128 to deal with error in omxplayer with hi-def videos
[pi5]
gpu_mem=256
dtparam=uart0=on
[pi4]
gpu_mem=256
[pi3]
gpu_mem=128
[pi0]
gpu_mem=64
[pi02]
gpu_mem=128
[pi1]
gpu_mem=64
[pi2]
gpu_mem=64

[all]

EOF

        # snd_bcm2835 is loaded by Pi firmware via dtparam=audio=on before
        # userspace modprobe runs, so /etc/modprobe.d options don't apply.
        # Kernel-command-line params do. enable_hdmi=0 drops the legacy
        # HDMI audio device (vc4hdmi provides HDMI audio natively), so:
        #   - "bcm2835 Headphones" becomes card 0 (restores FPP 9 layout)
        #   - no duplicate HDMI ALSA entries
        #   - aplay never hits the bcm2835 HDMI path, which on some
        #     Pi4/Pi5 + monitor combinations re-trains the HDMI link and
        #     briefly blanks the display.
        echo "FPP - Updating SPI buffer size and audio device selection"
        sed -i 's/$/ spidev.bufsiz=102400 snd_bcm2835.enable_headphones=1 snd_bcm2835.enable_hdmi=0/' ${BOOTDIR}/cmdline.txt

        echo "FPP - Updating root partition device"
        sed -i 's/root=PARTUUID=[A-Fa-f0-9-]* /root=\/dev\/mmcblk0p2 /g' ${BOOTDIR}/cmdline.txt
        sed -i 's/PARTUUID=[A-Fa-f0-9]*-01/\/dev\/mmcblk0p1/g' /etc/fstab
        sed -i 's/PARTUUID=[A-Fa-f0-9]*-02/\/dev\/mmcblk0p2/g' /etc/fstab

        echo "FPP - Disabling fancy network interface names"
        sed -i -e 's/rootwait/rootwait net.ifnames=0 /' ${BOOTDIR}/cmdline.txt

        echo "FPP - Freeing up more space by removing unnecessary packages"
        apt_purge_installed \
            wolfram-engine sonic-pi minecraft-pi firmware-iwlwifi \
            libglusterfs0 mesa-va-drivers mesa-vdpau-drivers mesa-vulkan-drivers \
            mkvtoolnix ncurses-term poppler-data va-driver-all librados2 libcephfs2
        apt-get -y --purge autoremove

        echo "FPP - Clearing config of packages marked for deinstall"
        dpkg --get-selections | grep deinstall | while read package _; do
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
        # Only present when resolvconf is installed (pre-Trixie). Newer
        # Raspbian uses systemd-resolved / NetworkManager and has no such hooks.
        if [ -f /etc/network/if-up.d/000resolvconf ]; then
            sed -i -n 'H;${x;s/^\n//;s/esac\n/&\nif grep -qc "Generated by fpp" \/etc\/resolv.conf\; then\n\texit 0\nfi\n/;p}' /etc/network/if-up.d/000resolvconf
        fi
        if [ -f /etc/network/if-down.d/resolvconf ]; then
            sed -i -n 'H;${x;s/^\n//;s/esac\n/&\nif grep -qc "Generated by fpp" \/etc\/resolv.conf\; then\n\texit 0\nfi\n\n/;p}' /etc/network/if-down.d/resolvconf
        fi

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
        # gpiochip4 symlink rule blocks gpio expanders that should sit at
        # gpiochip4 from getting a dev entry.
        rm -f /usr/lib/udev/rules.d/60-gpiochip4.rules
    fi

    echo "FPP - Disabling getty on onboard serial ttyAMA0"
    systemctl disable serial-getty@ttyAMA0.service || true
    sed -i -e "s/console=serial0,115200 //" ${BOOTDIR}/cmdline.txt
    # autologin@.service only exists when the Pi image shipped with a
    # pre-configured auto-login user; Trixie Lite doesn't.
    if [ -f /etc/systemd/system/autologin@.service ]; then
        sed -i -e "s/autologin pi/autologin ${FPPUSER}/" /etc/systemd/system/autologin@.service
    fi
    rm -f "/etc/systemd/system/getty@tty1.service.d/autologin.conf"
    # Trixie Lite doesn't enable getty@tty1 by default and
    # systemd-getty-generator doesn't auto-fire it for the Pi's HDMI
    # console, so a freshly-booted Pi plugged into a monitor shows nothing.
    # Enable it explicitly so a login prompt appears.
    systemctl enable getty@tty1.service

    # Mask Raspberry Pi OS's userconfig.service (from the userconf-pi
    # package -- yes the unit is "userconfig" with a g, while the package
    # and on-disk script are "userconf-pi" / "userconf-service". Naming
    # is a mess.) It runs unconditionally on first boot, and when
    # /boot/firmware/userconf.txt is absent (which it is -- FPP creates
    # the fpp user itself) it pops a whiptail "Please enter new username"
    # dialog on tty8 and switches the active VT there. End result: a
    # blank HDMI screen on first boot waiting for a username we already
    # have. Mask so it never runs.
    systemctl mask userconfig.service 2>/dev/null || true
    rm -f /etc/systemd/system/getty@tty8.service.d/userconfsetup.conf
    # SSH banner installed by userconf-pi telling the user "no valid user
    # has been set up" -- they have, we did it. Raspberry Pi OS removes
    # this file after first-boot user setup completes; since we mask the
    # service, that cleanup never runs, so do it ourselves.
    rm -f /etc/ssh/sshd_config.d/rename_user.conf

    # /var/swap only exists when dphys-swapfile has been run; not on a fresh
    # Trixie Lite base image.
    if [ -f /var/swap ]; then
        swapoff /var/swap || true
        rm -f /var/swap
    fi
    rfkill unblock all || true
}

# Armbian-specific image-mode tweaks. Shared between FPPPLATFORM=Armbian and
# FPPPLATFORM=Debian+ISARMBIAN=true (older Armbian releases mis-detected).
apply_armbian_image_tweaks() {
    apt-get remove -y network-manager
    sed -i -e "s/overlays=/overlays=i2c0 spi-spidev /" /boot/armbianEnv.txt
    echo "param_spidev_spi_bus=0"   >> /boot/armbianEnv.txt
    echo "param_spidev_max_freq=25000000" >> /boot/armbianEnv.txt
    sed -i -e "s/ENABLED=true/ENABLED=false/" /etc/default/armbian-ramlog
}

setup_platform_armbian() {
    if $isimage; then
        apply_armbian_image_tweaks
    fi
}

setup_platform_debian() {
    if $isimage; then
        apt-get remove -y network-manager
        if [ "${ISARMBIAN:-false}" = "true" ]; then
            apply_armbian_image_tweaks
        fi
    fi
}

case "${FPPPLATFORM}" in
    'BeagleBone Black')  setup_platform_beaglebone_black ;;
    'BeagleBone 64')     setup_platform_beaglebone_64 ;;
    'Raspberry Pi')      setup_platform_raspberry_pi ;;
    'Armbian')           setup_platform_armbian ;;
    'Debian')            setup_platform_debian ;;
    'Ubuntu'|'Fedora')   ;;  # no platform-specific image tweaks
    *)                   echo "FPP - Unknown platform" ;;
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
# Use --system so /etc/gitconfig is updated -- the setting then applies to
# every user regardless of HOME. Without this, `sudo git ...` (which keeps
# HOME=/home/fpp) reads /home/fpp/.gitconfig where the safe.directory
# was never set, and git aborts with "dubious ownership".
git config --system --add safe.directory /opt/fpp
git config --system --add safe.directory '*'

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
# Scrub any embedded credentials from the git config. GitHub's
# actions/checkout writes a short-lived runner token into
# [http "https://github.com/"] extraheader; if that tree is rsynced
# into the image it ships a credential that (a) expires and breaks
# the user's first Upgrade, and (b) should never have been published.
# Safe to run even when no such header exists.
if [ -d /opt/fpp/.git ]; then
    git -C /opt/fpp config --local --remove-section http."https://github.com/" 2>/dev/null || true
    git -C /opt/fpp submodule foreach --quiet --recursive \
        'git config --local --remove-section http."https://github.com/" 2>/dev/null || true' || true
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
bash ${FPPDIR}/scripts/upgrade_config -notee


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
# Create the fpp user at UID/GID 1000 plus hardware group memberships.
# Evicts any pre-existing user/group at UID/GID 1000 first (stock pi/debian
# users shipped in various base images) so the fixed-UID adduser can't
# silently collide.
add_fpp_user() {
    local existing_user_1000 existing_group_1000
    existing_user_1000=$(getent passwd 1000 | cut -d: -f1 || true)
    if [ -n "$existing_user_1000" ] && [ "$existing_user_1000" != "${FPPUSER}" ]; then
        echo "FPP - Removing pre-existing UID 1000 user: ${existing_user_1000}"
        userdel -r "${existing_user_1000}" 2>/dev/null || userdel "${existing_user_1000}" || true
    fi
    existing_group_1000=$(getent group 1000 | cut -d: -f1 || true)
    if [ -n "$existing_group_1000" ] && [ "$existing_group_1000" != "${FPPUSER}" ]; then
        echo "FPP - Removing pre-existing GID 1000 group: ${existing_group_1000}"
        groupdel "${existing_group_1000}" || true
    fi

    echo "FPP - Adding ${FPPUSER} user"
    addgroup --gid 1000 ${FPPUSER}
    adduser --uid 1000 --home ${FPPHOME} --shell /bin/bash --ingroup ${FPPUSER} \
            --gecos "Falcon Player" --disabled-password ${FPPUSER}
    adduser ${FPPUSER} adm
    adduser ${FPPUSER} sudo
    # Hardware groups -- only present when the corresponding kernel drivers
    # or packages have populated /etc/group.
    for grp in gpio i2c spi; do
        if getent group "$grp" > /dev/null; then
            adduser ${FPPUSER} "$grp"
        fi
    done
    adduser ${FPPUSER} video
    adduser ${FPPUSER} audio
    adduser ${FPPUSER} dialout

    # Pre-seed the fpp user's password ("falcon") directly in /etc/shadow so
    # the install is self-contained; no interactive passwd prompt.
    sed -i -e 's/^fpp:.*:/fpp:\$y\$j9T\$w6Yvb\/z1wtJh6TvI1zs8u\.\$ZEhs\/1W7JZlCrVCiVvsjarXzf\.3kY7GFHEpWIOQMPN6:20285:0:99999:7:::/' /etc/shadow
}
add_fpp_user

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

configure_logging() {
    echo "FPP - Configuring log rotation"
    if [ -d /etc/logrotate.d ]; then
        cp /opt/fpp/etc/logrotate.d/* /etc/logrotate.d/
    fi
    if [ -f /etc/logrotate.conf ]; then
        sed -i -e "s/#compress/compress/" /etc/logrotate.conf
        sed -i -e "s/rotate .*/rotate 2/" /etc/logrotate.conf
    fi
    if [ -f /etc/logrotate.d/rsyslog ]; then
        sed -i -e "s/weekly/daily/" /etc/logrotate.d/rsyslog
    fi

    # Disable duplicate logging to save on disk space
    local facility
    for facility in 'auth,authpriv\.\*' 'cron\.\*' 'kern\.\*' 'mail\.\*' 'user\.\*'; do
        sed -i "/${facility}/s/^/# /" /etc/rsyslog.conf
    done
}

configure_ccache() {
    echo "FPP - Configuring ccache"
    # ccache 4.12+ uses the XDG paths by default:
    #   cache dir: /root/.cache/ccache
    #   config:    /root/.config/ccache/ccache.conf
    # Several settings (e.g. max_size) are only accepted in the config
    # file when it's at the XDG user path, NOT at $CCACHE_DIR/ccache.conf.
    # Clean up stale legacy-location configs so ccache doesn't trip over
    # them when CCACHE_CONFIGPATH isn't set in the environment.
    rm -f /root/.ccache/ccache.conf
    rm -f /root/.cache/ccache/ccache.conf
    mkdir -p /root/.cache/ccache /root/.config/ccache
    export CCACHE_CONFIGPATH=/root/.config/ccache/ccache.conf
    ccache -M 500M
    ccache --set-config=temporary_dir=/tmp
    # locale sloppiness keeps LANG/LC_ALL out of the cache key. Without it,
    # manifest keys shipped from the image-build chroot (LANG typically "C")
    # don't match Pi-runtime upgrades (LANG "en_US.UTF-8"), busting every hit.
    ccache --set-config=sloppiness=pch_defines,time_macros,locale
    ccache --set-config=hard_link=true
    ccache --set-config=pch_external_checksum=true
    # Fingerprint the compiler by its version + target triple rather than by
    # hashing its binary contents. The chroot-built cache ships in the image,
    # and the Pi's /usr/bin/g++ can end up with different bytes than what was
    # hashed in the chroot (post-install triggers, qemu-user quirks, or any
    # dpkg-reconfigure path can rewrite the file), invalidating the entire
    # shipped cache on first upgrade. -dumpversion + -dumpmachine is stable
    # across any such rewrite but still changes when the user actually upgrades
    # the compiler.
    ccache --set-config=compiler_check='%compiler% -dumpversion; %compiler% -dumpmachine'
    # Mirror the config to the fpp user so non-root builds see the same
    # settings. Use the XDG path, matching root's layout above.
    mkdir -p /home/fpp/.config/ccache
    cp /root/.config/ccache/ccache.conf /home/fpp/.config/ccache/ccache.conf
    chown -R fpp:fpp /home/fpp/.config
    unset CCACHE_CONFIGPATH
}

configure_logging
configure_ccache

configure_samba_ftp() {
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
}

finalize_image_pre_build() {
    echo "FPP - Adding missing exim4 & ntpsec log directory"
    mkdir -p /var/log/ntpsec
    chown ntpsec:ntpsec /var/log/ntpsec
    mkdir -p /var/log/exim4
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
    rm -f /var/log/exim4/paniclog
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
    # Config fstab to mount some filesystems as tmpfs.
    # Skip on x86_64 (desktop installs typically have ample swap) and on
    # Raspberry Pi (Raspbian ships its own /tmp / /var/tmp handling that we
    # don't want to fight with).
    ARCH=$(uname -m)
    if [ "$ARCH" != "x86_64" ] && [ "$FPPPLATFORM" != "Raspberry Pi" ]; then
        echo "FPP - Configuring tmpfs filesystems"
        cp -f /opt/fpp/etc/systemd/tmp.mount /lib/systemd/system
        systemctl unmask tmp.mount
        systemctl enable tmp.mount

        sed -i 's|tmpfs\s*/tmp\s*tmpfs.*||g' /etc/fstab
        sed -i 's|tmpfs\s*/var/tmp\s*tmpfs.*||g' /etc/fstab
    fi

    COMMENTED=""
    SDA1=$(lsblk -l | grep sda1 | awk '{print $7}')
    if [ -n ${SDA1} ]
    then
        COMMENTED="#"
    fi
    echo "#####################################" >> /etc/fstab
    echo "${COMMENTED}/dev/sda1     ${FPPHOME}/media  auto    defaults,nonempty,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500  0  0" >> /etc/fstab
    echo "#####################################" >> /etc/fstab

    #######################################
    #  Prefer IPv4
    echo "FPP - Prefer IPv4"
    echo "precedence ::ffff:0:0/96  100" >>  /etc/gai.conf

    # need to keep extra memory to process network packets
    echo "vm.min_free_kbytes=16384" >> /etc/sysctl.conf
}

if $isimage; then
    configure_samba_ftp
fi

#######################################
# Fix sudoers to not require password
echo "FPP - Giving ${FPPUSER} user sudo"
echo "${FPPUSER} ALL=(ALL:ALL) NOPASSWD: ALL" >> /etc/sudoers

if $isimage; then
    finalize_image_pre_build
fi

#######################################
# Apache setup: envvars, FPP-supplied site/status configs, module enable set,
# and a log-name tweak so our logrotate config picks up apache's error log.
configure_apache() {
    echo "FPP - Configuring Apache webserver"

    sed -i -e "s/APACHE_RUN_USER=.*/APACHE_RUN_USER=${FPPUSER}/"   /etc/apache2/envvars
    sed -i -e "s/APACHE_RUN_GROUP=.*/APACHE_RUN_GROUP=${FPPUSER}/" /etc/apache2/envvars
    sed -i -e "s#APACHE_LOG_DIR=.*#APACHE_LOG_DIR=${FPPHOME}/media/logs#" /etc/apache2/envvars
    # Listen on the IPv6 wildcard; with net.ipv6.bindv6only=0 (the
    # Linux default) this dual-binds and serves both v4 and v6 clients
    # from a single socket.
    sed -i -e "s/Listen 8080.*/Listen [::]:80/" -e "s/^Listen 80$/Listen [::]:80/" /etc/apache2/ports.conf

    cat /opt/fpp/etc/apache2.site   > /etc/apache2/sites-enabled/000-default.conf
    cat /opt/fpp/etc/apache2.status > /etc/apache2/mods-enabled/status.conf

    # libapache2-mod-php${ACTUAL_PHPVER} isn't in our install list (we use
    # php-fpm), so the mod-php may not be present to disable. That's fine.
    a2dismod php${ACTUAL_PHPVER} 2>/dev/null || true
    a2dismod mpm_prefork 2>/dev/null || true

    local mod
    for mod in mpm_event http2 cgi rewrite expires proxy proxy_http proxy_http2 \
               proxy_html headers; do
        a2enmod "$mod"
    done
    a2enmod proxy_fcgi setenvif
    a2enconf php${ACTUAL_PHPVER}-fpm

    # Rename default error log so our logrotate rules apply.
    sed -i -e "s/error\.log/apache2-base-error.log/" /etc/apache2/apache2.conf

    rm /etc/apache2/conf-enabled/other-vhosts-access-log.conf

    systemctl enable apache2.service
}
configure_apache


configure_ntp() {
    echo "FPP - Configuring NTP Daemon"
    # Clear all existing servers/pools and set the falconplayer pool
    sed -i '/^server.*/d' /etc/ntpsec/ntp.conf
    sed -i '/^pool.*/d' /etc/ntpsec/ntp.conf
    sed -i '$s/$/\npool falconplayer.pool.ntp.org iburst minpoll 8 maxpoll 12 prefer/' /etc/ntpsec/ntp.conf
}
configure_ntp


finalize_platform_beaglebone_64() {
    sed -i -e "s/#force_color_prompt=yes/force_color_prompt=yes/" /home/fpp/.bashrc

    # remove the udev rules that create the SoftAp interface on the bbbw and bbggw
    rm -f /etc/udev/rules.d/*SoftAp*

    echo 'GOVERNOR="schedutil"' > /etc/default/cpufrequtils
    echo "USB_UMTPRD_DISABLED=yes" >> /etc/default/bb-boot

    cd /opt/fpp/capes/drivers/pinctrl
    make -j ${CPUS}
    make install
    make clean

    cd /opt/fpp/capes/drivers/bb64
    make -j ${CPUS}
    make install
    make clean

    cp /boot/firmware/extlinux/extlinux.conf /boot/firmware/extlinux/extlinux.conf.orig
    cp extlinux/extlinux.conf /boot/firmware/extlinux/extlinux.conf
    cd ~

    systemctl disable console-setup.service
}
finalize_platform_beaglebone_black() {
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

    cd /opt/fpp/capes/drivers/pinctrl
    make -j ${CPUS}
    make install
    make clean

    sed -i -e "s/#force_color_prompt=yes/force_color_prompt=yes/" /home/fpp/.bashrc

    # adjust a bunch of settings in /boot/uEnv.txt
    sed -i -e "s+^#enable_uboot_overlays=\(.*\)+enable_uboot_overlays=1+g"  /boot/uEnv.txt
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

    # clean out some DRI files that are not needed at all on Beagle hardware
    local dri=/usr/lib/arm-linux-gnueabihf/dri
    rm -rf ${dri}/r?00* ${dri}/rad* ${dri}/nouveau* ${dri}/rockchip* ${dri}/st* \
           ${dri}/ili* ${dri}/imx* ${dri}/exy* ${dri}/etn* ${dri}/hx* \
           ${dri}/kirin* ${dri}/rcar* ${dri}/mediatek* ${dri}/tegra* ${dri}/komeda* \
           ${dri}/v3d* ${dri}/vc4* ${dri}/virtio* ${dri}/zink* ${dri}/lima* \
           ${dri}/panfrost* ${dri}/armada* ${dri}/mi0283qt* ${dri}/pl111* ${dri}/msm* \
           ${dri}/mcde* ${dri}/meson* ${dri}/ingenic* ${dri}/mali* ${dri}/mxs* \
           ${dri}/rep* ${dri}/sun* ${dri}/vmw*
}

# Runs AFTER the main make, so the new u-boot overwrites whatever the build
# may have touched. Stock u-boot on recent rcn-ee base images has been
# unreliable on some Beagles; install a known-good version.
finalize_platform_beaglebone_black_post_build() {
    /opt/fpp/bin.bbb/bootloader/install.sh
}
case "${FPPPLATFORM}" in
    "BeagleBone Black") finalize_platform_beaglebone_black ;;
    "BeagleBone 64")    finalize_platform_beaglebone_64 ;;
esac

install_fpp_services() {
    echo "FPP - Configuring FPP startup"
    cp /opt/fpp/etc/systemd/*.service /lib/systemd/system/
    if $isimage; then
        mkdir -p /etc/networkd-dispatcher/initialized.d
        cp -a /opt/fpp/etc/networkd-dispatcher/* /etc/networkd-dispatcher
    fi

    systemctl disable mosquitto
    systemctl daemon-reload

    local svc
    for svc in fppinit fpprtc fppoled fppd fpp_postnetwork \
               fpp-install-kiosk fpp-reboot; do
        systemctl enable ${svc}.service
    done
}

# Image-only tweaks that happen after FPP services are installed/enabled.
finalize_image_services() {
    cp /opt/fpp/etc/update-RTC /etc/cron.daily

    # Make sure the journal is large enough to store the full boot logs
    # but not get too large which starts slowing down journalling (and thus boot)
    if [ -f /etc/systemd/journald.conf ]; then
        sed -i -e "s/^.*SystemMaxUse.*/SystemMaxUse=96M/g" /etc/systemd/journald.conf
    fi

    rm -f /etc/systemd/network/*eth*
    rm -f /etc/systemd/network/*wlan*
    cp /opt/fpp/etc/systemd/network/* /etc/systemd/network
}

if $isimage; then
    rm -rf /usr/share/doc/*
    apt-get clean
fi

install_fpp_services

if $isimage; then
    finalize_image_services
fi


echo "FPP - Compiling binaries"
cd /opt/fpp/src/
make clean ; make -j ${CPUS} optimized


configure_hostapd() {
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
}

# Swap configuration:
#   - Raspberry Pi OS ships its own rpi-swap mechanism (/etc/rpi/swap.conf
#     + systemd-zram-setup@zram0.service + dev-zram0.swap + rpi-zram-writeback)
#     that handles zram setup automatically. Don't write zram-tools config
#     on Pi -- it's read by a service we'd be disabling, and would just be
#     dead clutter.
#   - On BeagleBone (BBB / BB64) we still drive zram via zram-tools because
#     rcn-ee Debian doesn't ship the rpi-swap mechanism.
configure_swap() {
    if [ "$FPPPLATFORM" == "Raspberry Pi" ]; then
        # Just sysctl tuning; rpi-swap handles the actual zram device.
        echo "vm.swappiness=1" >> /etc/sysctl.d/10-swap.conf
        echo "vm.vfs_cache_pressure=50" >> /etc/sysctl.d/10-swap.conf
        # RPi OS ships zram-tools enabled by default. Disable it so it
        # doesn't race with rpi-swap's systemd-zram-setup@zram0 over
        # /dev/zram0 at boot (both try to claim the device).
        systemctl disable zramswap 2>/dev/null || true
    else
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
    fi
}

finalize_image_post_build() {
    systemctl disable dnsmasq
    systemctl unmask hostapd
    systemctl disable hostapd

    rm -f /etc/resolv.conf
    ln -s /run/systemd/resolve/resolv.conf /etc/resolv.conf

    # Disable winbind as we don't use it and it causes long delays at login
    pam-auth-update --disable winbind
    systemctl disable winbind
    systemctl mask winbind
    sed -i -e "s/winbind//" /etc/nsswitch.conf

    # Stop all the pipewire daemons starting up when user fpp logs in.
    # Note: target is /dev/null (the systemd convention for masking a unit).
    # The old code had a typo ("/dev/mull") which just left dangling symlinks;
    # fortunately those also prevented the units from loading, so the
    # intended effect was preserved by accident.
    mkdir -p /home/fpp/.config/systemd/user
    local svc
    for svc in pipewire.socket pipewire.service pipewire-pulse.service \
               pipewire-pulse.socket wireplumber.service; do
        ln -sf /dev/null /home/fpp/.config/systemd/user/$svc
    done
    chown -R fpp:fpp /home/fpp/.config

    # Remove stock distro users; FPP has its own.
    rm -rf /home/pi
    rm -rf /home/debian
}

######################################
case "${FPPPLATFORM}" in
    "Raspberry Pi"|"BeagleBone Black"|"BeagleBone 64")
        configure_hostapd
        configure_swap
        ;;
esac

if $isimage; then
    finalize_image_post_build
fi
if [ "${FPPPLATFORM}" = "BeagleBone Black" ]; then
    finalize_platform_beaglebone_black_post_build
fi

generate_apache_csp() {
    mkdir -p /home/fpp/media/config/
    /opt/fpp/scripts/ManageApacheContentPolicy.sh regenerate
}

print_install_complete() {
    local endtime
    endtime=$(date)
    cat <<-EOF
=========================================================
FPP Install Complete.
Started : ${STARTTIME}
Finished: ${endtime}
=========================================================
You can reboot the system by changing to the '${FPPUSER} user with the
password 'falcon' and running the shutdown command.

su - ${FPPUSER}
sudo shutdown -r now

NOTE: If you are prepping this as an image for release,
remove the SSH keys before shutting down so they will be
rebuilt during the next boot.

su - ${FPPUSER}
sudo rm -rf /etc/ssh/ssh_host*key*
sudo shutdown -r now
=========================================================

EOF
}

generate_apache_csp
print_install_complete

cp /root/FPP_Install.* ${FPPHOME}/
chown fpp:fpp ${FPPHOME}/FPP_Install.*
