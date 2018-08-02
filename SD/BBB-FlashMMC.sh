#!/bin/bash -e

export PARTSIZE=""
export DOREBOOT="y"
if [ "$1" == "-s" ]; then
    PARTSIZE=$2
    shift; shift;
fi
if [ "$1" == "-noreboot" ]; then
  DOREBOOT="n"
  shift;
fi


DEVICE=$2
if [ "x${DEVICE}" == "x" ]; then
    DEVICE="/dev/mmcblk1"
fi


cylon_leds () {
        if [ -e /sys/class/leds/beaglebone\:green\:usr0/trigger ] ; then
                BASE=/sys/class/leds/beaglebone\:green\:usr
                echo none > ${BASE}0/trigger
                echo none > ${BASE}1/trigger
                echo none > ${BASE}2/trigger
                echo none > ${BASE}3/trigger

                STATE=1
                while : ; do
                        case $STATE in
                        1)      echo 255 > ${BASE}0/brightness
                                echo 0   > ${BASE}1/brightness
                                STATE=2
                                ;;
                        2)      echo 255 > ${BASE}1/brightness
                                echo 0   > ${BASE}0/brightness
                                STATE=3
                                ;;
                        3)      echo 255 > ${BASE}2/brightness
                                echo 0   > ${BASE}1/brightness
                                STATE=4
                                ;;
                        4)      echo 255 > ${BASE}3/brightness
                                echo 0   > ${BASE}2/brightness
                                STATE=5
                                ;;
                        5)      echo 255 > ${BASE}2/brightness
                                echo 0   > ${BASE}3/brightness
                                STATE=6
                                ;;
                        6)      echo 255 > ${BASE}1/brightness
                                echo 0   > ${BASE}2/brightness
                                STATE=1
                                ;;
                        *)      echo 255 > ${BASE}0/brightness
                                echo 0   > ${BASE}1/brightness
                                STATE=2
                                ;;
                        esac
                        sleep 0.1
                done
        fi
}


prepareBTRFSPartitions() {
    echo "---------------------------------------"
    echo "Creating partitions"
    echo ""

    # create partitions
    sfdisk --force ${DEVICE} <<-__EOF__
4M,96M,,*
100M,${PARTSIZE},,-
__EOF__

    blockdev --rereadpt ${DEVICE}  || true
    fdisk -l ${DEVICE}  || true

    echo "---------------------------------------"
    echo "Formating partitions"
    echo ""

    #format partitions
    mkfs.ext4 -F -O ^metadata_csum,^64bit ${DEVICE}p1  -L bootfs
    mkfs.btrfs -f ${DEVICE}p2 -L rootfs

    echo "---------------------------------------"
    echo "Creating mount points and mounting rootfs"
    echo ""

    #mount
    mkdir /tmp/rootfs
    mount -t btrfs -o noatime,nodiratime,compress-force=zstd ${DEVICE}p2 /tmp/rootfs
    mkdir /tmp/rootfs/boot
    mount -t ext4 -o noatime,nodiratime ${DEVICE}p1 /tmp/rootfs/boot
}
prepareEXT4Partitions() {
    echo "---------------------------------------"
    echo "Creating partitions"
    echo ""

    # create partitions
    sfdisk --force ${DEVICE} <<-__EOF__
4M,${PARTSIZE},,-
__EOF__

    blockdev --rereadpt ${DEVICE}  || true
    fdisk -l ${DEVICE}  || true

    echo "---------------------------------------"
    echo "Formating partitions"
    echo ""

    #format partitions
    mkfs.ext4 -F -O ^metadata_csum,^64bit ${DEVICE}p1  -L rootfs

    echo "---------------------------------------"
    echo "Creating mount points and mounting rootfs"
    echo ""

    #mount
    mkdir /tmp/rootfs
    mount -t ext4 -o noatime,nodiratime ${DEVICE}p1 /tmp/rootfs

}

adjustEnvBTRFS() {
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

    echo "${DEVICE}p2  /  btrfs  noatime,nodiratime,compress=zstd  0  1" > /tmp/rootfs/etc/fstab
    echo "${DEVICE}p1  /boot  ext4  defaults,noatime,nodiratime  0  2" >> /tmp/rootfs/etc/fstab
}
adjustEnvEXT4() {
    echo "${DEVICE}p1  /  ext4  defaults,noatime,nodiratime  0  1" > /tmp/rootfs/etc/fstab

}

if [ "$DOREBOOT" == "y" ]; then
    cylon_leds & CYLON_PID=$!
fi

set -o pipefail

echo "---------------------------------------"
echo "Installing bootloader "
echo ""

#install bootloader
dd if=/opt/backup/uboot/MLO of=${DEVICE} count=1 seek=1 conv=notrunc bs=128k
dd if=/opt/backup/uboot/u-boot.img of=${DEVICE} count=2 seek=1 conv=notrunc bs=384k

if [ "$1" = "ext4" ]; then
    prepareEXT4Partitions
else
    prepareBTRFSPartitions
fi


echo "---------------------------------------"
echo "Copy files rootfs"
echo ""

#copy files
time rsync -aAxv /ID.txt /bin /boot /dev /etc /home /lib /lost+found /media /mnt /opt /proc /root /run /sbin /srv /sys /tmp /usr /var /tmp/rootfs --exclude=/dev/* --exclude=/proc/* --exclude=/sys/* --exclude=/tmp/* --exclude=/run/* --exclude=/mnt/* --exclude=/media/* --exclude=/lost+found --exclude=/uEnv.txt || true

echo "---------------------------------------"
echo "Configure /boot"
echo ""

#configure /boot
cd /tmp/rootfs/boot
rm -f boot
ln -sf . boot
cd /

if [ "$1" = "ext4" ]; then
    adjustEnvEXT4
else
    adjustEnvBTRFS
fi

echo "---------------------------------------"
echo "Configure /etc/fstab"
echo ""

#configure fstab
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
find /tmp/rootfs/var/log/  -type f -delete
rm -rf /tmp/rootfs/home/fpp/media/logs/*

echo "---------------------------------------"
echo "Unmounting filesystems"
echo ""
umount -R /tmp/rootfs

echo "---------------------------------------"
echo "Done flashing eMMC, powering down"
echo ""
if [ "$DOREBOOT" == "y" ]; then
    systemctl poweroff || halt
fi

