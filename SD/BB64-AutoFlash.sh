#!/bin/sh

#script that can be added to kernel init= to have the BB64/PB2 Industrial
#flash the eMMC when powered on.   It will partition the device, possibly
#reboot to get the new patition table, then install FPP onto the eMMC
#and then power attempt to power down.  The power  down usually will
#fail, but the indicator lights will stop flashing/cycling to 
#indicate it's done

mount -o remount,rw /dev/mmcblk1p3 /  || true
mkdir -p /proc
mount -t proc proc /proc

mkdir -p /sys
mount -t sysfs sysfs /sys

PATH=$PATH:/bin:/sbin:/usr/bin:/usr/sbin
DEVICE=/dev/mmcblk0

mount  -t tmpfs /tmp

echo "---------------------------------------"
echo "Installing bootloader "
echo ""

#install bootloader
/opt/u-boot/bb-u-boot-pocketbeagle2/install-emmc.sh


/opt/fpp/SD/BBB-FlashMMC.sh -noreboot ext4


mkdir -p /mnt
mount ${DEVICE}p0 /mnt
sed -i "s|default flashEMMC|default microSD|g" /mnt/extlinux/extlinux.conf
umount /mnt

shutdown -h now


