#!/bin/sh

#script that can be added to /boot/uEnv.txt to have the BBB automatically
#flash the eMMC when powered on.   It will partition the device, possibly
#reboot to get the new patition table, then install FPP onto the eMMC
#and then power attempt to power down.  The power  down usually will
#fail, but the indicator lights will stop flashing/cycling to 
#indicate it's done

mount -o remount,rw /dev/mmcblk0p1 /  || true

PATH=$PATH:/bin:/sbin:/usr/bin:/usr/sbin
DEVICE=/dev/mmcblk1


mount  -t tmpfs /tmp

PARTS=$(/sbin/sfdisk -l ${DEVICE} | /bin/grep ${DEVICE}p | /usr/bin/wc -l)

echo "---------------------------------------"
echo "Installing bootloader "
echo ""

#install bootloader
dd if=/opt/backup/uboot/MLO of=${DEVICE} count=1 seek=1 conv=notrunc bs=128k
dd if=/opt/backup/uboot/u-boot.img of=${DEVICE} count=2 seek=1 conv=notrunc bs=384k


if [ "$PARTS" != "1" ]; then
    echo "---------------------------------------"
    echo "Partitioning "
    echo ""

    /sbin/sfdisk --force ${DEVICE} <<-__EOF__
4M,${PARTSIZE},,-
__EOF__

    blockdev --rereadpt ${DEVICE}  || true
    fdisk -l ${DEVICE}  || true

    reboot
fi

/opt/fpp/SD/BBB-FlashMMC.sh -noreboot ext4

mount ${DEVICE}p1 /tmp/rootfs
sed -i '/.*AutoFlash\.sh/d' /tmp/rootfs/boot/uEnv.txt
echo ""  >> /tmp/rootfs/boot/uEnv.txt
echo "cmdline=coherent_pool=1M net.ifnames=0 lpj=1990656 rng_core.default_quality=100 quiet rootwait" >> /tmp/rootfs/boot/uEnv.txt
umount /tmp/rootfs
shutdown -h now


