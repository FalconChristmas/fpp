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
    echo "New image type '${NEWTYPE}' does not match existing '${ORIGTYPE}'"
    umount /mnt
    exit 1;
fi

#make sure settings are re-applied after boot
echo "BootActions = \"settings\"" >> /home/fpp/media/settings

#remove files where the binary may not have changed (so rsync won't recopy)
#but the caps (getcap) might be different
rm -f /bin/ping

echo "----------"
echo "Mounting filesystems for copy"
mount -o bind / /mnt/mnt
mount -o bind /boot /mnt/mnt/boot
mount -t tmpfs tmpfs /mnt/tmp
mount -o bind /dev /mnt/dev
mount -o bind /proc /mnt/proc

if [ -f /home/fpp/media/tmp/keepOptFPP ]
then
    # If we are on master and keeping /opt/fpp, run the existing part2 script
    echo "keepOptFPP flag exists, script will not copy /opt/fpp from image"
    echo "Passing control to existing upgradeOS-part2.sh from /opt/fpp"
    stdbuf --output=0 --error=0 chroot /mnt /mnt/opt/fpp/SD/upgradeOS-part2.sh
else
    # Not keeping /opt/fpp, so run part2 included in the fppos image
    echo "Passing control to upgradeOS-part2.sh from fppos image"
    stdbuf --output=0 --error=0 chroot /mnt /opt/fpp/SD/upgradeOS-part2.sh
fi

echo "----------"
echo "Control returned from upgradeOS-part2.sh script, resuming upgradeOS-part1.sh"

echo "Copy done, unmounting filesystems"
sync
umount /mnt/proc
umount /mnt/dev
umount /mnt/tmp
umount /mnt/mnt/boot
umount /mnt/mnt

sync

echo "Please reboot if the system does not do so automatically"

exec 0>&- # close stdin
exec 1>&- # close stdout
exec 2>&- # close stderr
sleep 1
sync
