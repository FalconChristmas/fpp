#!/bin/bash


mount $1 /mnt

ORIGTYPE=$(</etc/fpp/platform)
NEWTYPE=$(</mnt/etc/fpp/platform)

if [ $ORIGTYPE != $NEWTYPE ]; then
    echo "Image of '${NEWTYPE}' does no match '${ORIGTYPE}'"
    umount /mnt
    exit 1;
fi

#make sure settings are re-applied after boot
echo "BootActions = \"settings\"" >> /home/fpp/media/settings

#remove files where the binary may not have changed (so rsync won't recopy)
#but the caps (getcap) might be different
rm -f /bin/ping

mount -o bind / /mnt/mnt
mount -o bind /boot /mnt/mnt/boot
mount -t tmpfs tmpfs /mnt/tmp
mount -o bind /dev /mnt/dev
mount -o bind /proc /mnt/proc

chroot /mnt /mnt/opt/fpp/SD/upgradeOS-part2.sh

echo "Done copy"
sync
umount /mnt/dev
umount /mnt/tmp
umount /mnt/mnt/boot
umount /mnt/mnt

sync

echo "Rebooting...."

exec 0>&- # close stdin
exec 1>&- # close stdout
exec 2>&- # close stderr
sleep 1
nohup reboot -f -f &
