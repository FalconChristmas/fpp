#!/bin/bash

. /opt/fpp/scripts/common
. /opt/fpp/scripts/functions

cd /

LEGACYBOOT=no
FPPBOOTSRCDIR=boot
FPPBOOTDESTDIR=boot
BOOTMOUNT=$(mount | grep /boot | awk '{print $3}')
if [ -d "/boot/firmware" ]
then
    # Upgrading to a Raspbian 12 or newer system which mounts /boot/firmware instead of /boot
    FPPBOOTSRCDIR=boot/firmware
    FPPBOOTDESTDIR=boot/firmware

    if [ "${BOOTMOUNT}" = "/mnt/boot" ]
    then
        LEGACYBOOT=yes
        FPPBOOTDESTDIR=boot
    fi
elif [ "${BOOTMOUNT}" = "/mnt/boot/firmware" ]
then
    # Running on Raspbian 12 or newer but appear to be downgrading to a pre-12 version
    FPPBOOTSRCDIR=boot
    FPPBOOTDESTDIR=boot/firmware
    LEGACYBOOT=downgrading
fi

echo "Running rsync to update boot file system:"
if [ "${FPPPLATFORM}" = "Raspberry Pi" ]
then
    BOOTSIZE=$(sfdisk -q -l -o size /dev/mmcblk0 2>/dev/null | grep M)
    BOOTSIZE=${BOOTSIZE::-1}
    if [ `echo "$BOOTSIZE < 120"|bc` -eq 1 ]; then
        # 2.x image, boot partition is tiny and cannot fit everything needed for Pi4 and higher so we need to exclude some more things
        rsync --outbuf=N -aAXxvc ${FPPBOOTSRCDIR}/ /mnt/${FPPBOOTDESTDIR}/ --delete-before --exclude=kernel8.img  --exclude=kernel7l.img --exclude=.Spotlight*
    else
        rsync --outbuf=N -aAXxvc ${FPPBOOTSRCDIR}/ /mnt/${FPPBOOTDESTDIR}/ --delete-before
    fi

    if [ "${LEGACYBOOT}" = "yes" ]
    then
        # We are converting a system which was mounting /boot to now mount /boot/firmware
        # Fixup boot partition mount point in /etc/fstab
        sed -e "s# /boot # /boot/firmware #" -i /mnt/etc/fstab

        # Unmount /mnt/boot so we can see the actual /boot directory and create the new mount point
        umount /mnt/boot
        mkdir /mnt/boot/firmware

        # Create some necessary links to the new file locations
        find /boot/ -maxdepth 1 -type l | while read LINK
        do
            LINK=$(basename ${LINK})
            ln -s firmware/${LINK} /mnt/boot/${LINK}
        done
    elif [ "${LEGACYBOOT}" = "downgrading" ]
    then
        # Downgrading from Raspbian 12 or higher to a an older pre-12 version
        # Fixup boot partition mount point in /etc/fstab
        sed -e "s# /boot/firmware # /boot #" -i /mnt/etc/fstab

        # Unmount /mnt/boot/firmware so we can remove the mountpoint
        umount /mnt/boot/firmware
        rmdir /mnt/boot/firmware

        # Clean up any firmware.bak since it will be hidden by the mount
        rm -rf /mnt/boot/firmware.bak 2> /dev/null

        # Remove any links under /boot, they'll be hidden by the mount anyway
        find /mnt/boot/ -maxdepth 1 -type l | while read LINK
        do
            LINK=$(basename ${LINK})
            rm /mnt/boot/${LINK}
        done
    fi
else
    # Beaglebone
    rsync -aAXxvc ${FPPBOOTSRCDIR}/ /mnt/${FPPBOOTDESTDIR}/ --delete-during
fi
echo

if [ "${FPPPLATFORM}" = "BeagleBone Black" ]; then
    # need to install the bootloader that goes with this version of the os
    echo "Updating Beagle boot loader:"
    /opt/fpp/bin.bbb/bootloader/install.sh
    echo
fi

# temporarily copy the ssh keys
echo "Saving system ssh keys"
mkdir tmp/ssh
cp -a mnt/etc/ssh/*key* tmp/ssh
echo

echo "Saving hostname"
mkdir tmp/etc
cp -a mnt/etc/hostname tmp/etc

#remove some files that rsync won't copy over as they have the same timestamp and size, but are actually different
#possibly due to ACL's or xtended attributes
echo "Force cleaning files which do not sync properly"
rm -f mnt/bin/ping
rm -f mnt/lib/arm-linux-gnueabihf/librtmp.so.1
rm -f mnt/usr/bin/dc mnt/usr/bin/bc mnt/usr/bin/hardlink mnt/usr/bin/lua5*

SKIPFPP=""
if [ -f /mnt/home/fpp/media/tmp/keepOptFPP ]
then
    echo "Preserving existing /opt/fpp and /root/.ccache"
    SKIPFPP="--exclude=opt/fpp --exclude=root/.ccache"
fi

#if kiosk was installed, save that state so after reboot, it can be re-installed
KIOSKMODE=$(getSetting Kiosk)
if [ "${KIOSKMODE}" = "1" ]; then
    touch /tmp/kiosk
fi

#copy everything other than fstab and the persistent net names
echo "Running rsync to update / (root) file system:"
stdbuf --output=L --error=L rsync --outbuf=N -aAXxv bin etc lib opt root sbin usr var /mnt --delete-during --exclude=var/lib/php/sessions --exclude=etc/fstab --exclude=etc/systemd/network/*-fpp-* --exclude=root/.ssh ${SKIPFPP}
echo

#restore the ssh keys
echo "Restoring system ssh keys"
cp -a tmp/ssh/* mnt/etc/ssh
echo

echo "Restoring hostname"
cp -af tmp/etc/hostname mnt/etc/hostname
rm -f  tmp/etc/hostname
echo 

#create a file in root to mark it as requiring kiosk mode to be installed, will be checked on reboot
if [ -f tmp/kiosk ]; then
    touch mnt/fpp_kiosk
fi

if [ -f mnt/etc/ssh/ssh_host_dsa_key -a -f mnt/etc/ssh/ssh_host_dsa_key.pub -a -f mnt/etc/ssh/ssh_host_ecdsa_key -a -f mnt/etc/ssh/ssh_host_ecdsa_key.pub -a -f mnt/etc/ssh/ssh_host_ed25519_key -a -f mnt/etc/ssh/ssh_host_ed25519_key.pub -a -f mnt/etc/ssh/ssh_host_rsa_key -a -f mnt/etc/ssh/ssh_host_rsa_key.pub ]
then
    echo "Found all SSH key files, disabling first-boot SSH key regeneration"
    rm mnt/etc/systemd/system/multi-user.target.wants/regenerate_ssh_host_keys.service
fi


echo "Running sync command to flush data"
sync

echo "Sync complete"
sleep 3

exit
