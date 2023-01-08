#!/bin/bash

FPPOS=`/usr/bin/basename $1`
GITHUBSIZE=`curl -fsSL http://127.0.0.1/api/git/releases/sizes | grep ${FPPOS} | awk -F, '{print $2}'`
OURSIZE=`/usr/bin/stat -c %s $1`

if ! [[ $GITHUBSIZE =~ ^-?[0-9]+$ ]];
then
  echo "Couldn't get fppos size from Github, attempting upgrade anyway"
else
  if [ "$OURSIZE" -lt "$GITHUBSIZE" ];
  then
    echo "Download size seems too small. Our size: $OURSIZE, Github size: $GITHUBSIZE deleting $1"
    echo "Please try to download the fppos again"
    rm $1
    exit 1;
  else
    echo "fppos size matches Github, continuing"
  fi
fi

mount $1 /mnt

ORIGTYPE=$(</etc/fpp/platform)
NEWTYPE=$(</mnt/etc/fpp/platform)

if [ "$ORIGTYPE" != "$NEWTYPE" ]; then
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

stdbuf --output=0 --error=0 chroot /mnt /opt/fpp/SD/upgradeOS-part2.sh

echo "Done copy, unmounting filesystems"
sync
umount /mnt/dev
umount /mnt/tmp
umount /mnt/mnt/boot
umount /mnt/mnt

sync

echo "Please Reboot...."

exec 0>&- # close stdin
exec 1>&- # close stdout
exec 2>&- # close stderr
sleep 1
sync
