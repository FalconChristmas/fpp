#! /bin/sh

FPPBOOTDIR=/boot
if [ -d "/boot/firmware" ]
then
    FPPBOOTDIR=/boot/firmware
fi

echo "Checking for updated eeprom to allow booting from USB"
echo ""
rpi-eeprom-update -a

if [ -f "${FPPBOOTDIR}/recovery.bin" ]; then
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
    mount /dev/sda1 /mnt${FPPBOOTDIR}

    echo "program_usb_boot_mode=1" >> /mnt${FPPBOOTDIR}/config.txt
    sed -i 's/root=\/dev\/[a-zA-Z0-9]* /root=\/dev\/sda2 /g' /mnt${FPPBOOTDIR}/cmdline.txt
    sed -i 's/LABEL=boot[a-zA-Z0-9]* /LABEL=bootsda /g' /mnt/etc/fstab
    sed -i 's/LABEL=root[a-zA-Z0-9]* /LABEL=rootfssda /g' /mnt/etc/fstab
elif [ "$DEVICE" = "nvme0n1" ]; then
    fatlabel /dev/nvme0n1p1 bootnvme
    e2label /dev/nvme0n1p2 rootfsnvme
    mount /dev/nvme0n1p2 /mnt
    mount /dev/nvme0n1p1 /mnt${FPPBOOTDIR}

    # Set BOOT_ORDER in the EEPROM configuration
    BOOT_ORDER=0xf61

    # Create a temporary configuration file
    echo -e "[all]\nBOOT_ORDER=${BOOT_ORDER}" > /tmp/boot.conf
    
    # Apply the configuration using the rpi-eeprom-config tool
    sudo rpi-eeprom-config --config /tmp/boot.conf --out /tmp/new-eeprom.bin
    
    # Flash the EEPROM with the new configuration
    sudo rpi-eeprom-update -d -f /tmp/new-eeprom.bin
    
    # Clean up
    rm /tmp/boot.conf /tmp/new-eeprom.bin
    
    echo "BOOT_ORDER set to ${BOOT_ORDER}."

    sed -i 's/root=\/dev\/[a-zA-Z0-9]* /root=\/dev\/nvme0n1p2 /g' /mnt${FPPBOOTDIR}/cmdline.txt
    sed -i 's/LABEL=boot[a-zA-Z0-9]* /LABEL=bootnvme /g' /mnt/etc/fstab
    sed -i 's/LABEL=root[a-zA-Z0-9]* /LABEL=rootfsnvme /g' /mnt/etc/fstab
else 
    fatlabel /dev/mmcblk0p1 boot
    e2label /dev/mmcblk0p2 rootfs
    mount /dev/mmcblk0p2 /mnt
    mount /dev/mmcblk0p1 /mnt${FPPBOOTDIR}

    sed -i 's/root=\/dev\/[a-zA-Z0-9]* /root=\/dev\/mmcblk0p2 /g' /mnt${FPPBOOTDIR}/cmdline.txt
    sed -i 's/LABEL=boot[a-zA-Z0-9]* /LABEL=boot /g' /mnt/etc/fstab
    sed -i 's/LABEL=root[a-zA-Z0-9]* /LABEL=rootfs /g' /mnt/etc/fstab
    sed -i  "s/^program_usb_boot_mode=.*//g" /mnt${FPPBOOTDIR}/config.txt
fi

if [ "$ISCLONE" = "0" ]; then
    rm -rf /mnt/var/log/*
    mkdir /mnt/var/log/ntpsec
    chown ntpsec:ntpsec /mnt/var/log/ntpsec
    mkdir /mnt/var/log/exim4
    chown Debian-exim /mnt/var/log/exim4
    chgrp Debian-exim /mnt/var/log/exim4

    rm /mnt/home/fpp/.bash_history
fi
#umount /mnt${FPPBOOTDIR}
#umount /mnt

echo ""
echo ""
echo ""
echo ""
if [ "$ISCLONE" = "1" ]; then
    echo "******  Clone to $DEVICE complete.  ******"
else
    echo "******  Flash to $DEVICE complete.  ******"
fi 

echo ""

