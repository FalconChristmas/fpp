#!/bin/bash

FPPOS=`/usr/bin/basename $1`
GITHUBSIZE=`curl -fsSL http://127.0.0.1/api/git/releases/sizes | grep ${FPPOS} | awk -F, '{print $2}'`
OURSIZE=`/usr/bin/stat -c %s $1`

# Locate the boot partition, if there is a separate one.  `mount -o bind /` below
# is NOT recursive, so anything mounted under / is invisible through that bind and
# has to be bound in on its own.  What matters here is therefore "where is the boot
# partition mounted", not "where do the boot files live":
#   Raspberry Pi / BeagleBone 64 -> a FAT partition mounted on /boot/firmware
#   older images                 -> a boot partition mounted on /boot
#   BeagleBone Black             -> no separate boot partition at all; /boot is a
#                                   directory on the root filesystem and is already
#                                   covered by the bind of /
# The BBB is why this probes mounts rather than testing for a directory: current
# BBB images ship a /boot/firmware directory (ID.txt, START.HTM, sysconf.txt) that
# is not a mount and is not where the boot files live, so `-d /boot/firmware` bound
# the wrong path and left the real boot files unhandled.
#
# Not exported: upgradeOS-part2.sh uses a BOOTMOUNT of its own with a different
# meaning, and it assigns before it reads.
BOOTMOUNT=""
for __d in /boot/firmware /boot; do
    if findmnt -n "${__d}" > /dev/null 2>&1; then
        BOOTMOUNT="${__d}"
        break
    fi
done
unset __d

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
if [ -n "${BOOTMOUNT}" ]; then
    mount -o bind ${BOOTMOUNT} /mnt/mnt${BOOTMOUNT}
fi
mount -t tmpfs tmpfs /mnt/tmp
mount -o bind /dev /mnt/dev
mount -o bind /proc /mnt/proc

if [ -f /home/fpp/media/tmp/keepOptFPP ]
then
    # If we are on master and keeping /opt/fpp, run the existing part2 script
    echo "keepOptFPP flag exists, script will not copy /opt/fpp from image."
    echo "Passing control to existing upgradeOS-part2.sh from /opt/fpp"
    stdbuf --output=0 --error=0 chroot /mnt /mnt/opt/fpp/SD/upgradeOS-part2.sh
elif [ "${BOOTMOUNT}" = "/boot/firmware" -a ! -d "/mnt/boot/firmware" ]
then
    # Downgrading from Raspbian 12 or higher to a pre-12 version without /boot/firmware.
    # Keyed off BOOTMOUNT rather than `-d /boot/firmware` so the BBB -- which has that
    # directory but boots from /boot -- never lands here.
    echo "Downgrading to OS version without /boot/firmware."
    echo "Passing control to upgradeOS-part2.sh from current version."
    cp /opt/fpp/SD/upgradeOS-part2.sh /home/fpp/media/tmp/upgradeOS-part2.sh
    stdbuf --output=0 --error=0 chroot /mnt /mnt/home/fpp/media/tmp/upgradeOS-part2.sh
    rm /home/fpp/media/tmp/upgradeOS-part2.sh
else
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
if [ -n "${BOOTMOUNT}" ]; then
    umount /mnt/mnt${BOOTMOUNT}
fi
umount /mnt/mnt

sync

echo "Please reboot if the system does not do so automatically"

exec 0>&- # close stdin
exec 1>&- # close stdout
exec 2>&- # close stderr
sleep 1
sync
