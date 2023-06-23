#! /bin/sh

echo "Checking for updated eeprom to allow booting from USB"
echo ""
rpi-eeprom-update -a

if [ -f "/boot/recovery.bin" ]; then
    echo ""
    echo ""
    echo "<b>EEPROM update required before flashing.  Please reboot to apply updated EEPROM.</b>"
    echo ""
    exit 0
fi


/opt/fpp/SD/rpi-clone -v -U sda

fatlabel /dev/sda1 boot
e2label /dev/sda2 rootfs

mount /dev/sda2 /mnt
mount /dev/sda1 /mnt/boot

echo "program_usb_boot_mode=1" >> /mnt/boot/config.txt
sed -i 's/root=\/dev\/mmcblk0p[0-9]* /root=\/dev\/sda2 /g' /mnt/boot/cmdline.txt

umount /mnt/boot
umount /mnt
