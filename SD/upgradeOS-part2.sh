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
        rsync -aAxv boot /mnt --delete-before --exclude=boot/kernel8.img  --exclude=boot/kernel7l.img --exclude=boot/.Spotlight*
    else
        rsync -aAxv boot /mnt --delete-before
    fi
else
    rsync -aAxv boot /mnt --delete-during
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
mkdir tmp/ssh
cp -a mnt/etc/ssh/*key* tmp/ssh

#copy everything other than fstab
rsync -aAxv bin etc lib opt root sbin usr var /mnt --delete-during --exclude=etc/fstab

#restore the ssh keys
cp -a tmp/ssh/* mnt/etc/ssh

exit
