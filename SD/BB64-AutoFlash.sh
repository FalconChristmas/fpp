#!/bin/sh

#script that can be added to kernel init= to have the BB64/PB2 Industrial
#flash the eMMC when powered on.   It will partition the device, possibly
#reboot to get the new patition table, then install FPP onto the eMMC
#and then power down.  The indicator lights also stop flashing/cycling to
#indicate it's done, which is the signal to trust if the board is still powered
#for any reason.

mount -o remount,rw /dev/mmcblk1p3 /  || true
mkdir -p /proc
mount -t proc proc /proc

mkdir -p /sys
mount -t sysfs sysfs /sys

PATH=$PATH:/bin:/sbin:/usr/bin:/usr/sbin
DEVICE=/dev/mmcblk0

# This runs as init, so udev is never started -- and udev is what loads the driver
# module for an i2c client, off the MODALIAS in its uevent.  The kernel creates the
# clients from the device tree either way, so without this the identity EEPROM and
# the ADC beside it sit on the bus with no driver bound and no /sys attributes:
# indistinguishable, from sysfs alone, from an MSPM0 that was never programmed.
# Both are modules in the FPP kernel (CONFIG_EEPROM_AT24=m, CONFIG_AD7291=m).
for mod in at24 ad7291; do
    modprobe ${mod} 2>/dev/null || true
done

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
    # This script IS init, so exiting is a kernel panic ("Attempted to kill init"),
    # and with no panic= on the command line the board just wedges -- no reboot, and
    # nothing left to ask questions of.  Hand the serial console a shell instead: the
    # LEDs keep blinking from the kernel's timer trigger either way, so the board
    # still reads as failed to anyone who only has the lights.
    echo ""
    echo "Dropping to a shell on the console.  The board stays unflashed."
    exec /bin/sh -i < /dev/console > /dev/console 2>&1
    # If /dev/console is not usable, hang rather than panic.
    while true; do sleep 3600; done
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
# the only remaining state -- and it is what remains readable if the power off below
# does not take, since a board that never powered down still has to look finished.
for led in /sys/class/leds/*usr[0-9]; do
    [ -e "$led/trigger" ] || continue
    echo none > "$led/trigger" 2>/dev/null
    echo 0 > "$led/brightness" 2>/dev/null
done

# Shut down.  Note what is NOT available here: /sbin/{shutdown,halt,poweroff,reboot}
# are all symlinks to systemctl, and this script is PID 1, so there is no systemd for
# any of them to talk to.  sysrq is the only route out.
echo 1 > /proc/sys/kernel/sysrq

# sysrq 'o' hands off to orderly_poweroff(), which first runs poweroff_cmd as a
# usermode helper -- /sbin/poweroff, i.e. systemctl again -- and only forces a real
# kernel_power_off() once that fails.  Point it at something that cannot exist so it
# fails immediately instead of grinding through that.
echo /nonexistent-force-kernel-poweroff > /proc/sys/kernel/poweroff_cmd 2>/dev/null || true

echo s > /proc/sysrq-trigger   # sync
echo u > /proc/sysrq-trigger   # remount everything read-only
echo o > /proc/sysrq-trigger   # power off

# 'o' is asynchronous -- it queues work and the write returns straight away -- so the
# power off is still being set up when this line is reached.  Waiting here is not
# politeness: this script IS init, so running off the end is "Attempted to kill
# init", a kernel panic that beats the shutdown to the finish and lands on a board
# that flashed perfectly.
#
# The power off itself is real on this hardware.  The TPS65219 PMIC at i2c 0x30 is
# marked system-power-controller in the device tree, so the tps65219 driver registers
# a SYS_OFF_MODE_POWER_OFF handler and the kernel can actually cut the rails -- there
# is no psci node, but none is needed, the PMIC is the handler.  So this loop should
# never get more than a few seconds in.  It is the backstop for the case where the
# handler does not fire, where a parked init with everything synced and read-only,
# and the LEDs dark, is still a correct and readable end state.
while true; do
    sleep 3600
done


