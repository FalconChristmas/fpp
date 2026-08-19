#!/bin/sh

#script that can be added to kernel init= to have the BB64/PB2 Industrial
#flash the eMMC when powered on.   It will partition the device, possibly
#reboot to get the new patition table, then install FPP onto the eMMC
#and then power attempt to power down.  The power  down usually will
#fail, but the indicator lights will stop flashing/cycling to 
#indicate it's done

mount -o remount,rw /dev/mmcblk1p3 /  || true
mkdir -p /proc
mount -t proc proc /proc

mkdir -p /sys
mount -t sysfs sysfs /sys

PATH=$PATH:/bin:/sbin:/usr/bin:/usr/sbin
DEVICE=/dev/mmcblk0

mount -t vfat /dev/mmcblk1p1 /boot/firmware
mount  -t tmpfs /tmp

# Mark a failure the only two ways an unattended flash can be read: leave the board
# powered on, and blink every user LED together.  A finished board goes dark, so
# neither can be mistaken for success.  flash_storage.sh does this for its own
# failures; this covers the steps either side of it.  Note the PocketBeagle2 exposes
# usr1-usr4 where the BeagleBone Black exposes usr0-usr3, hence the glob.
#
# Defined before the first thing that can fail: the EEPROM check below aborts boards
# with no EEPROM at all, and sh resolves a function only once it has been read.
fail() {
    echo ""
    echo "############################################################"
    echo "#  FLASH FAILED -- this board is NOT ready and must not be"
    echo "#  shipped.  $1"
    echo "############################################################"
    for led in /sys/class/leds/*usr[0-9]; do
        [ -e "$led/trigger" ] || continue
        echo timer > "$led/trigger" 2>/dev/null || continue
        echo 120 > "$led/delay_on" 2>/dev/null
        echo 120 > "$led/delay_off" 2>/dev/null
    done
    exit 1
}

echo "---------------------------------------"

# Some PB2i boards ship with a blank or half-programmed EEPROM, which makes u-boot
# configure the industrial board as the 512MB base one.  check_pb2_eeprom.sh works
# out which variant this is, repairs what can be repaired, and verifies the result.
#
# It exits non-zero when the EEPROM is missing entirely or is programmed with the
# wrong identity.  Neither can be written around, and a board flashed in that state
# boots misidentified -- so stop here and blink, rather than producing something
# that looks finished.
/bin/bash /opt/fpp/capes/drivers/bb64/check_pb2_eeprom.sh \
    || fail "The on-board EEPROM is not usable; see the message above."

echo "---------------------------------------"
echo "Installing bootloader "
echo ""

#install bootloader
/opt/u-boot/bb-u-boot-pocketbeagle2/install-emmc.sh


# flash_storage.sh partitions, copies, configures and then VERIFIES the eMMC.
/opt/fpp/SD/flash_storage.sh -y --clone --no-reboot ${DEVICE} \
    || fail "See the errors above."

# flash_storage.sh's bb64 fixup already does both of these; repeated here because
# they are idempotent and a board that boots to the flasher again is unrecoverable
# without a serial console.
mkdir -p /mnt
mount ${DEVICE}p1 /mnt || fail "could not mount the flashed eMMC boot partition."
sed -i "s|default flashEMMC|default microSD|g" /mnt/extlinux/extlinux.conf
# Don't need to check if we have to expand the FS on the eMMC
rm -f /mnt/fpp_expand_rootfs
umount /mnt

# Done.  A sweep means working and a synchronised blink means failed, so darkness is
# the only remaining state -- and unlike "powered off" it survives the shutdown below
# failing, which in this environment it usually does.
for led in /sys/class/leds/*usr[0-9]; do
    [ -e "$led/trigger" ] || continue
    echo none > "$led/trigger" 2>/dev/null
    echo 0 > "$led/brightness" 2>/dev/null
done

echo 1 > /proc/sys/kernel/sysrq
echo s > /proc/sysrq-trigger
echo u > /proc/sysrq-trigger
echo o > /proc/sysrq-trigger


