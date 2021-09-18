#!/bin/bash

. /opt/fpp/scripts/common
. /opt/fpp/scripts/functions

cd /

if [ "${FPPPLATFORM}" = "Raspberry Pi" ]
then
    BOOTSIZE=$(sfdisk -q -l -o size /dev/mmcblk0 2>/dev/null | grep M)
    BOOTSIZE=${BOOTSIZE::-1}
    if [ `echo "$BOOTSIZE < 120"|bc` -eq 1 ]; then
        # 2.x image, boot partition is tiny and cannot fit everything needed for Pi4 so we need to exclude some more things
        rsync --outbuf=N -aAXxv boot /mnt --delete-before --exclude=boot/kernel8.img  --exclude=boot/kernel7l.img --exclude=boot/.Spotlight*
    else
        rsync --outbuf=N -aAXxv boot /mnt --delete-before
    fi
else
    rsync -aAXxv boot /mnt --delete-during
fi

if [ "${FPPPLATFORM}" = "BeagleBone Black" ]; then
    # need to install the bootloader that goes with this version of the os
    cd /opt/backup/uboot
    dd if=MLO of=/dev/mmcblk1 seek=1 bs=128k
    dd if=u-boot.img of=/dev/mmcblk1 seek=1 bs=384k
    dd if=MLO of=/dev/mmcblk0 seek=1 bs=128k
    dd if=u-boot.img of=/dev/mmcblk0 seek=1 bs=384k
    cd /
fi

# temporarily copy the ssh keys
echo "Saving ssh keys"
mkdir tmp/ssh
cp -a mnt/etc/ssh/*key* tmp/ssh

#copy everything other than fstab and the persistent net names
stdbuf --output=L --error=L rsync --outbuf=N -aAXxv bin etc lib opt root sbin usr var /mnt --delete-after --exclude=var/lib/connman --exclude=var/lib/php/sessions --exclude=etc/fstab --exclude=etc/systemd/network/*-fpp-*

#restore the ssh keys
echo "Restoring ssh keys"
cp -a tmp/ssh/* mnt/etc/ssh

sync
sleep 3

echo ""
echo ""

exit
