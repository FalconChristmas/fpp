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


/opt/fpp/SD/rpi-clone -v -U sda

fatlabel /dev/sda1 bootsda
e2label /dev/sda2 rootfssda

mount /dev/sda2 /mnt
mount /dev/sda1 /mnt/boot

echo "program_usb_boot_mode=1" >> /mnt/boot/config.txt
sed -i 's/root=\/dev\/mmcblk0p[0-9]* /root=\/dev\/sda2 /g' /mnt/boot/cmdline.txt
sed -i 's/LABEL=boot /LABEL=bootsda /g' /mnt/etc/fstab
sed -i 's/LABEL=rootfs /LABEL=rootfssda /g' /mnt/etc/fstab

umount /mnt/boot
umount /mnt

echo ""
echo ""
echo ""
echo ""
echo "******  Flash to USB complete.  Shutdown and remove SD card to boot from USB. ******"
echo ""

