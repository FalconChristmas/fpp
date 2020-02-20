#!/bin/bash

mount $1 /mnt


ORIGTYPE=$(</etc/fpp/platform)
NEWTYPE=$(</mnt/etc/fpp/platform)

if [ $ORIGTYPE != $NEWTYPE ]; then
    echo "Image of '${NEWTYPE}' does no match '${ORIGTYPE}'"
    umount /mnt
    exit 1;
fi

mount -o bind / /mnt/mnt
mount -o bind /boot /mnt/mnt/boot
mount -t tmpfs tmpfs /mnt/tmp
mount -o bind /dev /mnt/dev

chroot /mnt /mnt/opt/fpp/SD/upgradeOS-part2.sh

echo "Done copy"
sync
umount /mnt/dev
umount /mnt/tmp
umount /mnt/mnt/boot
umount /mnt/mnt

sync

echo "</pre>"
echo "<b>Rebooting.... please wait for FPP to reboot.</b><br>"
echo "<a href='index.php'>Go to FPP Main Status Page</a><br>"
echo "<a href='about.php'>Go back to FPP About page</a><br>"
echo "</body>"
echo "</html>"

reboot -f -f
