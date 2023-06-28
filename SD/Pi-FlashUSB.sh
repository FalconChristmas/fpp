#! /bin/sh

echo "Checking for updated eeprom to allow booting from USB"
echo ""
rpi-eeprom-update -a

if [ -f "/boot/recovery.bin" ]; then
    echo ""
    echo ""
    echo "******  EEPROM update required before flashing.  Please reboot to apply updated EEPROM.  ******"
    echo ""
    exit 0
fi

export EXCLUDE_FLAGS="--exclude=/home/fpp/media/* --exclude=/etc/ssh/*key*"
export ISCLONE=0
if [ "$1" = "-clone" ]; then
    EXCLUDE_FLAGS=""
    ISCLONE=1
    shift
fi

export DEVICE=$1

/opt/fpp/SD/rpi-clone -v -U $EXCLUDE_FLAGS $DEVICE


if [ "$DEVICE" = "sda" ]; then
    fatlabel /dev/sda1 bootsda
    e2label /dev/sda2 rootfssda
    mount /dev/sda2 /mnt
    mount /dev/sda1 /mnt/boot

    echo "program_usb_boot_mode=1" >> /mnt/boot/config.txt
    sed -i 's/root=\/dev\/[a-zA-Z0-9]* /root=\/dev\/sda2 /g' /mnt/boot/cmdline.txt
    sed -i 's/LABEL=boot[a-zA-Z0-9]* /LABEL=bootsda /g' /mnt/etc/fstab
    sed -i 's/LABEL=root[a-zA-Z0-9]* /LABEL=rootfssda /g' /mnt/etc/fstab
else 
    fatlabel /dev/mmcblk0p1 boot
    e2label /dev/mmcblk0p2 rootfs
    mount /dev/mmcblk0p2 /mnt
    mount /dev/mmcblk0p1 /mnt/boot

    sed -i 's/root=\/dev\/[a-zA-Z0-9]* /root=\/dev\/mmcblk0p2 /g' /mnt/boot/cmdline.txt
    sed -i 's/LABEL=boot[a-zA-Z0-9]* /LABEL=boot /g' /mnt/etc/fstab
    sed -i 's/LABEL=root[a-zA-Z0-9]* /LABEL=rootfs /g' /mnt/etc/fstab
    sed -i  "s/^program_usb_boot_mode=.*//g" /mnt/boot/config.txt
fi

if [ "$ISCLONE" = "0" ]; then
    rm /mnt/var/log/*
    rm /mnt/home/fpp/.bash_history
fi
umount /mnt/boot
umount /mnt

echo ""
echo ""
echo ""
echo ""
echo "******  Flash to $DEVICE complete.  s******"
echo ""

