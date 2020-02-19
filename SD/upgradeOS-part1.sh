#!/bin/bash

mount $1 /mnt

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

reboot -f -f
