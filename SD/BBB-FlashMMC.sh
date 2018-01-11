#!/bin/sh

DEVICE="/dev/mmcblk1"

echo "---------------------------------------"
echo "Creating partitions"
echo ""

# create partitions
sfdisk --force ${DEVICE} <<-__EOF__
4M,124M,,*
128M,,,-
__EOF__

blockdev --rereadpt ${DEVICE}
fdisk -l ${DEVICE}

echo "---------------------------------------"
echo "Formating partitions"
echo ""


#format partitions
mkfs.ext4 -F -O ^metadata_csum,^64bit ${DEVICE}p1  -L bootfs
mkfs.btrfs -f ${DEVICE}p2 -L rootfs

echo "---------------------------------------"
echo "Installing bootloader "
echo ""

#install bootloader
dd if=/opt/backup/uboot/MLO of=/dev/mmcblk1 count=1 seek=1 conv=notrunc bs=128k
dd if=/opt/backup/uboot/u-boot.img of=/dev/mmcblk1 count=2 seek=1 conv=notrunc bs=384k

echo "---------------------------------------"
echo "Creating mount points and mounting rootfs"
echo ""

#mount
mkdir /tmp/rootfs
mount -t btrfs -o noatime,nodiratime,compress-force=zlib ${DEVICE}p2 /tmp/rootfs
mkdir /tmp/rootfs/boot
mount -t ext4 -o noatime ${DEVICE}p1 /tmp/rootfs/boot

echo "---------------------------------------"
echo "Copy files rootfs"
echo ""

#copy files
rsync -aAx --human-readable --info=name0,progress2 /ID.txt /bin /boot /dev /etc /home /lib /lost+found /media /mnt /opt /proc /root /run /sbin /srv /sys /tmp /usr /var /tmp/rootfs --exclude=/dev/* --exclude=/proc/* --exclude=/sys/* --exclude=/tmp/* --exclude=/run/* --exclude=/mnt/* --exclude=/media/* --exclude=/lost+found --exclude=/uEnv.txt

echo "---------------------------------------"
echo "Configure /boot"
echo ""

#configure /boot
cd /tmp/rootfs/boot
ln -s . boot
cd /

cd /opt/source/bb.org-overlays
make clean
make

mkdir /tmp/rootfs/boot/lib
mkdir /tmp/rootfs/boot/lib/firmware
cp /opt/source/bb.org-overlays/src/arm/* /tmp/rootfs/boot/lib/firmware

echo ""  >> /tmp/rootfs/boot/uEnv.txt
echo "mmcrootfstype=btrfs" >> /tmp/rootfs/boot/uEnv.txt
echo "rootfstype=btrfs" >> /tmp/rootfs/boot/uEnv.txt
echo "mmcpart=2" >> /tmp/rootfs/boot/uEnv.txt
echo ""  >> /tmp/rootfs/boot/uEnv.txt

echo "---------------------------------------"
echo "Configure /etc/fstab"
echo ""

#configure fstab
echo "/dev/mmcblk1p2  /  btrfs  noatime,nodiratime,compress-force=zlib  0  1" > /tmp/rootfs/etc/fstab
echo "/dev/mmcblk1p1  /boot  ext4  defaults,noatime,nodiratime  0  2" >> /tmp/rootfs/etc/fstab
echo "debugfs  /sys/kernel/debug  debugfs  defaults  0  0" >> /tmp/rootfs/etc/fstab
echo "tmpfs         /tmp        tmpfs   nodev,nosuid,size=10M 0 0" >> /tmp/rootfs/etc/fstab
echo "tmpfs         /var/tmp    tmpfs   nodev,nosuid,size=50M 0 0" >> /tmp/rootfs/etc/fstab
echo "#####################################" >> /tmp/rootfs/etc/fstab
echo "#/dev/sda1     /home/fpp/media  auto    defaults,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500  0  0" >> /tmp/rootfs/etc/fstab
echo "#####################################" >> /tmp/rootfs/etc/fstab

echo "---------------------------------------"
echo "Cleaning up"
echo ""
rm -rf /tmp/rootfs/etc/ssh/*key*
rm -rf /tmp/rootfs/var/lib/connman/eth*
rm /tmp/rootfs/var/log/*
rm /tmp/rootfs/var/log/samba/*
rm /tmp/rootfs/home/fpp/media/logs/*


echo "---------------------------------------"
echo "Unmounting filesystems"
echo ""
umount /tmp/rootfs/boot
umount /tmp/rootfs

echo "---------------------------------------"
echo "Done flashing eMMC, powering down"
echo ""
systemctl poweroff || halt

