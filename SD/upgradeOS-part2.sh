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
        rsync -aAxv boot /mnt --delete-before  --exclude=boot/cmdline.txt --exclude=boot/kernel8.img  --exclude=boot/kernel7l.img --exclude=boot/.Spotlight*
    else
        rsync -aAxv boot /mnt --delete-before  --exclude=boot/cmdline.txt
    fi
else
    rsync -aAxv boot /mnt --delete-during
fi
rsync -aAxv bin etc lib opt root sbin usr var /mnt --delete-during --exclude=etc/fstab
