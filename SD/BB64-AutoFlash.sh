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

echo "---------------------------------------"

# Some PB2i boards ship with blank or improperly-programmed EEPROMs.
# The factory write is two-step: (1) board template, (2) serial number.
# Either or both steps can fail, so check both:
#   - Magic header (bytes 0-1) should be AA55
#   - Serial number (bytes 52-55) should not be all FF
# The fix scripts use a merge strategy that only overwrites 0xFF bytes
# in the existing EEPROM, so they're safe to run in either failure mode.
EEPROM=/sys/bus/i2c/devices/0-0050/eeprom
NEEDS_FIX=false
if [ -f "$EEPROM" ]; then
    MAGIC=$(dd if="$EEPROM" bs=1 count=2 2>/dev/null | xxd -p)
    SERIAL=$(dd if="$EEPROM" bs=1 skip=52 count=4 2>/dev/null | xxd -p)
    if [ "$MAGIC" != "aa55" ]; then
        echo "EEPROM header invalid (expected aa55, got ${MAGIC:-empty})"
        NEEDS_FIX=true
    elif [ "$SERIAL" = "ffffffff" ]; then
        echo "EEPROM header valid but serial number is blank"
        NEEDS_FIX=true
    else
        echo "EEPROM valid (magic=${MAGIC}, serial=${SERIAL})"
    fi

    if $NEEDS_FIX; then
        # Determine the board variant.  The device-tree model string is NOT
        # usable for this: it reads "BeagleBoard.org PocketBeagle2" on the
        # industrial board as well as on the base one.
        #
        # The board ID at EEPROM offset 46 is "PB2I" on the industrial and
        # "PB20" on the base board, and it is still readable in the "valid
        # header, blank serial" failure mode.  When the header itself is blank
        # there is nothing left to read, so probe the hardware instead: the
        # industrial always has eMMC populated, the base board ships with the
        # footprint empty (and is not a target for this script anyway, since
        # everything below flashes ${DEVICE}).
        #
        # This matters beyond cosmetics.  u-boot's SPL only applies the 1GB
        # DDRSS timings when the EEPROM identifies the board as the industrial
        # variant, so writing a base-board EEPROM onto an industrial leaves it
        # running on half its RAM until the EEPROM is rewritten.
        BOARDID=""
        if [ "$MAGIC" = "aa55" ]; then
            BOARDID=$(dd if="$EEPROM" bs=1 skip=46 count=4 2>/dev/null)
        fi
        case "$BOARDID" in
            PB2I)
                INDUSTRIAL=true
                ;;
            PB20)
                INDUSTRIAL=false
                ;;
            *)
                # Blank header - wait briefly for the eMMC to enumerate, then
                # decide on its presence.
                INDUSTRIAL=false
                i=0
                while [ $i -lt 10 ]; do
                    if [ "$(cat /sys/block/mmcblk0/device/type 2>/dev/null)" = "MMC" ]; then
                        INDUSTRIAL=true
                        break
                    fi
                    i=$((i + 1))
                    sleep 1
                done
                echo "EEPROM header blank, detected industrial=${INDUSTRIAL} from eMMC presence"
                ;;
        esac

        if $INDUSTRIAL; then
            echo "Running fix_pb2i_eeprom.sh"
            /bin/bash /opt/fpp/capes/drivers/bb64/fix_pb2i_eeprom.sh
        else
            echo "Running fix_pb2_eeprom.sh"
            /bin/bash /opt/fpp/capes/drivers/bb64/fix_pb2_eeprom.sh
        fi
    fi
else
    echo "WARNING: EEPROM device not found at $EEPROM"
fi

echo "---------------------------------------"
echo "Installing bootloader "
echo ""

#install bootloader
/opt/u-boot/bb-u-boot-pocketbeagle2/install-emmc.sh


/opt/fpp/SD/BBB-FlashMMC.sh -noreboot ext4


mkdir -p /mnt
mount ${DEVICE}p1 /mnt
sed -i "s|default flashEMMC|default microSD|g" /mnt/extlinux/extlinux.conf
# Don't need to check if we have to expand the FS on the eMMC
rm -f /mnt/fpp_expand_rootfs
umount /mnt

echo 1 > /proc/sys/kernel/sysrq
echo s > /proc/sysrq-trigger
echo u > /proc/sysrq-trigger
echo o > /proc/sysrq-trigger


