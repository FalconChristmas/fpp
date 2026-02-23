#!/bin/bash -e

ARCH=$(uname -m)

export PARTSIZE=""
export DOREBOOT="y"
export CLEARFPP="n"

if [ "$1" == "-s" ]; then
    PARTSIZE=$2
    shift; shift;
fi
if [ "$1" == "-noreboot" ]; then
  DOREBOOT="n"
  shift;
fi

if [ "$1" == "-clear" ]; then
  CLEARFPP="y"
  shift;
fi

DEVICE=$2
if [ "x${DEVICE}" == "x" ]; then
    if [ "$ARCH" == "aarch64" ]; then
        DEVICE="/dev/mmcblk0"
    else
        DEVICE="/dev/mmcblk1"
    fi
fi

DEVSIZE=$(blockdev --getsz ${DEVICE})


cylon_leds () {
        if [ -e /sys/class/leds/beaglebone\:green\:usr0/trigger ] ; then
                BASE=/sys/class/leds/beaglebone\:green\:usr
                ARCH=$(uname -m)
                if [ "${ARCH}" = "aarch64" ] ; then
                    BASE0=${BASE}1
                    BASE1=${BASE}2
                    BASE2=${BASE}3
                    BASE3=${BASE}4
                else
                    BASE0=${BASE}0
                    BASE1=${BASE}1
                    BASE2=${BASE}2
                    BASE3=${BASE}3
                fi

                echo "none" > ${BASE0}/trigger
                echo "none" > ${BASE1}/trigger
                echo "none" > ${BASE2}/trigger
                echo "none" > ${BASE3}/trigger

                STATE=1
                while : ; do
                        case $STATE in
                        1)      echo "255" > ${BASE0}/brightness
                                echo "0"   > ${BASE1}/brightness
                                STATE=2
                                ;;
                        2)      echo "255" > ${BASE1}/brightness
                                echo "0"   > ${BASE0}/brightness
                                STATE=3
                                ;;
                        3)      echo "255" > ${BASE2}/brightness
                                echo "0"   > ${BASE1}/brightness
                                STATE=4
                                ;;
                        4)      echo "255" > ${BASE3}/brightness
                                echo "0"   > ${BASE2}/brightness
                                STATE=5
                                ;;
                        5)      echo "255" > ${BASE2}/brightness
                                echo "0"   > ${BASE3}/brightness
                                STATE=6
                                ;;
                        6)      echo "255" > ${BASE1}/brightness
                                echo "0"   > ${BASE2}/brightness
                                STATE=1
                                ;;
                        *)      echo "255" > ${BASE0}/brightness
                                echo "0"   > ${BASE1}/brightness
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
    mkdir -p /tmp/rootfs
    mount -t btrfs -o noatime,nodiratime,compress-force=zstd ${DEVICE}p2 /tmp/rootfs
    mkdir -p /tmp/rootfs/boot
    mount -t ext4 -o noatime,nodiratime ${DEVICE}p1 /tmp/rootfs/boot
}


prepareThreePartitions() {
    echo "---------------------------------------"
    echo "Creating partitions"
    echo ""

if [ "$ARCH" == "aarch64" ]; then
    # create partitions
    sfdisk --force ${DEVICE} <<-__EOF__
1M,256M,c,*
257M,1024M,82,*
1281M,${PARTSIZE},,-
__EOF__
else
    # create partitions
    sfdisk --force ${DEVICE} <<-__EOF__
4M,36M,c,*
40M,512M,82,*
542M,${PARTSIZE},,-
__EOF__
fi

    blockdev --rereadpt ${DEVICE}  || true
    fdisk -l ${DEVICE}  || true

    echo "---------------------------------------"
    echo "Formating partitions"
    echo ""

    #format partitions
    mkfs.vfat -F 16 ${DEVICE}p1  -n boot
    mkfs.ext4 -F -O ^metadata_csum,^64bit ${DEVICE}p3  -L rootfs
    #mkfs.btrfs -f ${DEVICE}p3 -L rootfs
    mkswap ${DEVICE}p2 -L swap

    echo "---------------------------------------"
    echo "Creating mount points and mounting rootfs"
    echo ""

    #mount
    mkdir -p /tmp/rootfs
    mount -t ext4 -o noatime,nodiratime ${DEVICE}p3 /tmp/rootfs
    #mount -t btrfs -o noatime,nodiratime,compress-force=zstd ${DEVICE}p3 /tmp/rootfs
    mkdir -p /tmp/rootfs/boot
    mkdir -p /tmp/rootfs/boot/firmware
    mount -t vfat -o noatime,nodiratime ${DEVICE}p1 /tmp/rootfs/boot/firmware
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
    mkdir -p /tmp/rootfs
    mount -t ext4 -o noatime,nodiratime ${DEVICE}p1 /tmp/rootfs

}

adjustEnvBTRFS() {
    cd /opt/source/bb.org-overlays
    make clean
    make

    mkdir -p /tmp/rootfs/boot/lib
    mkdir -p /tmp/rootfs/boot/lib/firmware
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

    if [ "$PARTSIZE" == "" ] && [ "$DEVSIZE" -gt 9000000 ]; then
        echo "${DEVICE}p3  /  ext4  defaults,noatime,nodiratime  0  1" > /tmp/rootfs/etc/fstab
        echo "${DEVICE}p1  /boot/firmware vfat user,uid=1000,gid=1000,defaults 0 2" >> /tmp/rootfs/etc/fstab
        echo "${DEVICE}p2       none    swap    sw      0       0" >>  /tmp/rootfs/etc/fstab
    else 
        echo "${DEVICE}p1  /  ext4  defaults,noatime,nodiratime  0  1" > /tmp/rootfs/etc/fstab
    fi


}

if [ "$DOREBOOT" == "y" ]; then
    cylon_leds & CYLON_PID=$!
fi

set -o pipefail



if [ "$ARCH" == "aarch64" ]; then
    prepareThreePartitions
    
    #install bootloader
    #cd /opt/u-boot/bb-u-boot-pocketbeagle2/
    #./install-emmc.sh
else
    echo "---------------------------------------"
    echo "Installing bootloader "
    echo ""

    #install bootloader
    if [ -f  /opt/fpp/bin.bbb/bootloader/install.sh ]; then
    /opt/fpp/bin.bbb/bootloader/install.sh
    fi

    if [ "$1" = "btrfs" ]; then
        prepareBTRFSPartitions
    else
        if [ "$PARTSIZE" == "" ] && [ "$DEVSIZE" -gt 9000000 ]; then
            prepareThreePartitions
        else 
            prepareEXT4Partitions
        fi
    fi
fi


echo "---------------------------------------"
echo "Copy files rootfs"
echo ""

#copy files
time rsync -aAXxv /ID.txt /bin /boot /dev /etc /home /lib /lost+found /media /mnt /opt /proc /root /run /sbin /srv /sys /tmp /usr /var /tmp/rootfs --exclude=/dev/* --exclude=/proc/* --exclude=/sys/* --exclude=/tmp/* --exclude=/run/* --exclude=/mnt/* --exclude=/media/* --exclude=/lost+found --exclude=/uEnv.txt || true

if [ "$ARCH" == "aarch64" ]; then
    cp -a /boot/firmware/* /tmp/rootfs/boot/firmware/ || true

    mkdir -p /tmp/rootfs/etc

    echo "---------------------------------------"
    echo "Configure /etc/fstab"
    echo ""
    echo "${DEVICE}p3  /  ext4  defaults,noatime,nodiratime,errors=remount-ro  0  1" > /tmp/rootfs/etc/fstab
    echo "${DEVICE}p2       none    swap    sw      0       0" >> /tmp/rootfs/etc/fstab
    echo "${DEVICE}p1  /boot/firmware vfat user,uid=1000,gid=1000,defaults 0 2" >> /tmp/rootfs/etc/fstab
    echo "debugfs  /sys/kernel/debug  debugfs  mode=755,uid=root,gid=gpio,defaults  0  0" >> /tmp/rootfs/etc/fstab
    echo "#####################################" >> /tmp/rootfs/etc/fstab
    echo "#/dev/sda1     /home/fpp/media  auto    defaults,nonempty,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500  0  0" >> /tmp/rootfs/etc/fstab
    echo "#####################################" >> /tmp/rootfs/etc/fstab

    sed -i "s|root=/dev/[a-zA-Z0-9]*\([0-9]\) |root=${DEVICE}p3 |g" /tmp/rootfs/boot/firmware/extlinux/extlinux.conf
    sed -i "s|resume=/dev/[a-zA-Z0-9]*\([0-9]\) |resume=${DEVICE}p2 |g" /tmp/rootfs/boot/firmware/extlinux/extlinux.conf
    #sed -i "s|ext4|btrfs|g" /tmp/rootfs/boot/firmware/extlinux/extlinux.conf
    
else
    echo "---------------------------------------"
    echo "Configure /boot"
    echo ""

    #configure /boot
    cd /tmp/rootfs/boot

    #remove btrfs stuff from uEnv.txt (may be re-added later)
    sed -i '/mmcpart=2/d' uEnv.txt
    sed -i '/rootfstype=btrfs/d' uEnv.txt
    sed -i '/mmcrootfstype=btrfs/d' uEnv.txt

    rm -f boot
    ln -sf . boot
    cd /

    if [ "$1" = "btrfs" ]; then
        adjustEnvBTRFS
    else
        adjustEnvEXT4
    fi

    echo "---------------------------------------"
    echo "Configure /etc/fstab"
    echo "  "

    #configure fstab
    echo "debugfs  /sys/kernel/debug  debugfs  defaults  0  0" >> /tmp/rootfs/etc/fstab
    echo "#####################################" >> /tmp/rootfs/etc/fstab
    echo "#/dev/sda1     /home/fpp/media  auto    defaults,noatime,nodiratime,exec,nofail,flush,uid=1000,gid=1000  0  0" >> /tmp/rootfs/etc/fstab
    echo "#####################################" >> /tmp/rootfs/etc/fstab

    echo "---------------------------------------"
    echo "Cleaning up"
    echo "  "
fi
rm -rf /tmp/rootfs/etc/ssh/*key*


rm -rf /tmp/rootfs/var/lib/connman/eth*
find /tmp/rootfs/var/log/  -type f -delete
rm -rf /tmp/rootfs/home/fpp/media/logs/*
rm -f /tmp/rootfs/root/.bash_history
rm -f /tmp/rootfs/root/.wget-hsts
echo "uninitialized" > /tmp/rootfs/etc/machine-id

if [ "$CLEARFPP" == "y" ]; then
    rm -f /tmp/rootfs/home/fpp/.bash_history
    rm -f /tmp/rootfs/home/fpp/media/settings
    rm -rf /tmp/rootfs/home/fpp/media/backups/*
    rm -rf /tmp/rootfs/home/fpp/media/config/backups
    rm -f /tmp/rootfs/home/fpp/media/config/*
    rm -f /tmp/rootfs/home/fpp/media/sequences/*
    rm -f /tmp/rootfs/home/fpp/media/music/*
    rm -f /tmp/rootfs/home/fpp/media/logs/*
    rm -f /tmp/rootfs/home/fpp/media/tmp/*
    rm -f /tmp/rootfs/home/fpp/media/images/*
    rm -f /tmp/rootfs/home/fpp/media/effects/*
    rm -f /tmp/rootfs/home/fpp/media/cache/*
    rm -f /tmp/rootfs/home/fpp/media/playlists/*
    rm -f /tmp/rootfs/home/fpp/media/plugins/*
    rm -f /tmp/rootfs/home/fpp/media/scripts/*
    rm -f /tmp/rootfs/home/fpp/media/videos/*
    rm -f /tmp/rootfs/home/fpp/media/uploads/*
fi



echo "---------------------------------------"
echo "Unmounting filesystems"
echo "  "
umount -R /tmp/rootfs/boot/firmware || true
umount -R /tmp/rootfs

echo "---------------------------------------"
echo "Done flashing eMMC, powering down"
echo ""
if [ "$DOREBOOT" == "y" ]; then
    systemctl poweroff || halt
fi
