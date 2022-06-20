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
        rsync --outbuf=N -aAXxvc boot /mnt --delete-before --exclude=boot/kernel8.img  --exclude=boot/kernel7l.img --exclude=boot/.Spotlight*
    else
        rsync --outbuf=N -aAXxvc boot /mnt --delete-before
    fi
else
    rsync -aAXxvc boot /mnt --delete-during
fi

if [ "${FPPPLATFORM}" = "BeagleBone Black" ]; then
    # need to install the bootloader that goes with this version of the os
    /opt/fpp/bin.bbb/bootloader/install.sh
fi

# temporarily copy the ssh keys
echo "Saving ssh keys"
mkdir tmp/ssh
cp -a mnt/etc/ssh/*key* tmp/ssh

#remove some files that rsync won't copy over as they have the same timestamp and size, but are actually different
#possibly due to ACL's or xtended attributes
rm -f mnt/bin/ping
rm -f mnt/lib/arm-linux-gnueabihf/librtmp.so.1
rm -f mnt/usr/bin/dc mnt/usr/bin/bc mnt/usr/bin/hardlink mnt/usr/bin/lua5*

#copy everything other than fstab and the persistent net names
stdbuf --output=L --error=L rsync --outbuf=N -aAXxv bin etc lib opt root sbin usr var /mnt --delete-after --exclude=var/lib/connman --exclude=var/lib/php/sessions --exclude=etc/fstab --exclude=etc/systemd/network/*-fpp-* --exclude=root/.ssh

#restore the ssh keys
echo "Restoring ssh keys"
cp -a tmp/ssh/* mnt/etc/ssh

sync
sleep 3

echo ""
echo ""

exit
