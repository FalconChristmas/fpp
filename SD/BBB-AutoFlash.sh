#!/bin/sh

#script that can be added to /boot/uEnv.txt to have the BBB automatically
#flash the eMMC when powered on.   It will partition the device, possibly
#reboot to get the new patition table, then install FPP onto the eMMC
#and then power attempt to power down.  The power  down usually will
#fail, but the indicator lights will stop flashing/cycling to 
#indicate it's done

mount -o remount,rw /dev/mmcblk0p1 /  || true
mkdir -p /proc
mount -t proc proc /proc

mkdir -p /sys
mount -t sysfs sysfs /sys

PATH=$PATH:/bin:/sbin:/usr/bin:/usr/sbin
DEVICE=/dev/mmcblk1


mount  -t tmpfs /tmp

echo "---------------------------------------"
echo "Installing bootloader "
echo ""

#install bootloader
/opt/fpp/bin.bbb/bootloader/install.sh


/opt/fpp/SD/BBB-FlashMMC.sh -noreboot ext4

PARTS=$(/sbin/sfdisk -l ${DEVICE} | /bin/grep ${DEVICE}p | /usr/bin/wc -l)

mkdir -p /tmp/rootfs
mount ${DEVICE}p${PARTS} /tmp/rootfs
sed -i '/.*AutoFlash\.sh/d' /tmp/rootfs/boot/uEnv.txt
echo ""  >> /tmp/rootfs/boot/uEnv.txt
echo "cmdline=coherent_pool=1M net.ifnames=0 lpj=1990656 rng_core.default_quality=100 quiet rootwait" >> /tmp/rootfs/boot/uEnv.txt
rm -f /tmp/rootfs/boot/BBB-*
umount /tmp/rootfs
shutdown -h now


