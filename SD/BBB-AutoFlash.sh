#!/bin/sh

#script that can be added to /boot/uEnv.txt to have the BBB automatically
#flash the eMMC when powered on.   It will partition the device, possibly
#reboot to get the new patition table, then install FPP onto the eMMC
#and then power attempt to power down.  The power  down usually will
#fail, but the indicator lights will stop flashing/cycling to 
#indicate it's done

mount -o remount,rw /dev/mmcblk0p1 /  || true
mkdir -p /proc
mount -t proc proc /proc

mkdir -p /sys
mount -t sysfs sysfs /sys

PATH=$PATH:/bin:/sbin:/usr/bin:/usr/sbin
DEVICE=/dev/mmcblk1


mount  -t tmpfs /tmp

# Mark a failure the only two ways an unattended flash can be read: leave the board
# powered on, and blink every user LED together.  A finished board goes dark, so
# neither can be mistaken for success.  flash_storage.sh does this for its own
# failures; this covers the steps after it.
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

# flash_storage.sh partitions, copies, configures and then VERIFIES the eMMC, and
# installs the bootloader itself once that verification passes.  It strips this
# AutoFlash hook and the BBB-* helpers from the copy's uEnv.txt as part of its fixup.
/opt/fpp/SD/flash_storage.sh -y --clone --no-reboot ${DEVICE} \
    || fail "See the errors above."

PARTS=$(/sbin/sfdisk -l ${DEVICE} | /bin/grep ${DEVICE}p | /usr/bin/wc -l)

mkdir -p /tmp/rootfs
mount ${DEVICE}p${PARTS} /tmp/rootfs || fail "could not mount the flashed eMMC."
# The quieter cmdline the shipped image boots with -- this is the one piece that is
# specific to factory flashing rather than to copying FPP, so it stays here.
echo ""  >> /tmp/rootfs/boot/uEnv.txt
echo "cmdline=coherent_pool=1M net.ifnames=0 lpj=1990656 rng_core.default_quality=100 quiet rootwait" >> /tmp/rootfs/boot/uEnv.txt
umount /tmp/rootfs

# Done.  A sweep means working and a synchronised blink means failed, so darkness is
# the only remaining state -- and unlike "powered off" it survives the shutdown below
# failing, which in this environment it usually does.
for led in /sys/class/leds/*usr[0-9]; do
    [ -e "$led/trigger" ] || continue
    echo none > "$led/trigger" 2>/dev/null
    echo 0 > "$led/brightness" 2>/dev/null
done

shutdown -h now


