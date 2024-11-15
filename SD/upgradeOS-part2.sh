#!/bin/bash

. /opt/fpp/scripts/common
. /opt/fpp/scripts/functions

cd /

echo "Running rsync to update boot file system:"
if [ "${FPPPLATFORM}" = "Raspberry Pi" ]
then
    ROOTDEV=$(findmnt -T /mnt | tail -n 1 | awk '{print $2}');
    BOOTMOUNT=$(mount | grep /boot | awk '{print $3}')
    BOOTMOUNTDEV=$(findmnt -T /mnt/boot | tail -n 1 | awk '{print $2}')
    # Upgrading to a Raspbian 12 or newer system which mounts /boot/firmware instead of /boot
    if [ "${BOOTMOUNT}" = "/mnt/boot" ]; then
        echo "Need to remount /mnt/boot to /mnt/boot/firmware"
        umount /mnt/boot
        mkdir -p /mnt/boot/firmware
        mount ${BOOTMOUNTDEV} /mnt/boot/firmware
    fi

    rsync --outbuf=N -aAXxvc /boot/ /mnt/boot/ --delete-before

    if [ "${BOOTMOUNT}" = "/mnt/boot" ]; then
        echo "Adjusting /etc/fstab"
        # We are converting a system which was mounting /boot to now mount /boot/firmware
        # Fixup boot partition mount point in /etc/fstab
        sed -e "s# /boot # /boot/firmware #" -i /mnt/etc/fstab
    fi
    sed -i "s|root=/dev/[a-zA-Z0-9]* |root=${ROOTDEV} |g" /mnt/boot/firmware/cmdline.txt
else
    # Beaglebone
    rsync -aAXxvc /boot/ /mnt/boot/ --delete-during
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

echo "Saving machine-id"
cp -a mnt/etc/machine-id tmp/etc


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

# force copy a few files that rsync cannot properly replace
cp -af usr/lib/arm-linux-gnueabihf/libzip.so.4.0 mnt/usr/lib/arm-linux-gnueabihf/libzip.so.4.0
cp -af usr/lib/arm-linux-gnueabihf/libfribidi.so.0.4.0 mnt/usr/lib/arm-linux-gnueabihf/libfribidi.so.0.4.0
cp -af usr/lib/arm-linux-gnueabihf/libbrotlicommon.so.1.0.9 mnt/usr/lib/arm-linux-gnueabihf/libbrotlicommon.so.1.0.9

#restore the ssh keys
echo "Restoring system ssh keys"
cp -a tmp/ssh/* mnt/etc/ssh
echo

echo "Restoring hostname"
cp -af tmp/etc/hostname mnt/etc/hostname
rm -f  tmp/etc/hostname
echo 

echo "Restoring machine-id"
cp -af tmp/etc/machine-id mnt/etc/machine-id
rm -f  tmp/etc/machine-id
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
