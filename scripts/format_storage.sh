#!/bin/bash
FS=$1
DEVICE=$2
# Validate DEVICE — only allow real block device names, no shell metachars or paths.
# This prevents injection like "sda1; rm -rf /" when run as root.
if ! [[ "$DEVICE" =~ ^(sd[a-z][0-9]+|mmcblk[0-9]+p[0-9]+|nvme[0-9]+n[0-9]+p[0-9]+)$ ]]; then
    echo "Invalid device: $DEVICE" >&2
    exit 1
fi
# Split trailing partition digits: sda10 -> sda + 10, mmcblk0p10 -> mmcblk0p + 10,
# nvme0n1p10 -> nvme0n1p + 10. A fixed-width cut breaks on multi-digit
# partitions (sda10 parsed as RAWDEV=sda1 PARTNUM=0, retargeting sfdisk).
if [[ $DEVICE =~ ^(.*[^0-9])([0-9]+)$ ]]; then
    RAWDEV=${BASH_REMATCH[1]}
    PARTNUM=${BASH_REMATCH[2]}
    # mmcblk/nvme use a 'p' separator between disk and partition number;
    # strip it only for those families (an sd name like sdp1 keeps its p).
    if [[ $DEVICE == mmcblk* || $DEVICE == nvme* ]]; then
        RAWDEV=${RAWDEV%p}
    fi
else
    echo "Unable to split device/partition: $DEVICE" >&2
    exit 1
fi

echo "$RAWDEV"   "$PARTNUM"
# Allowed values mirror www/formatstorage.php (which validates before invoking
# us); anything else exits non-zero instead of silently formatting nothing.
if [ "$FS" != 'FAT' ] && [ "$FS" != 'ext4' ] && [ "$FS" != 'exFAT' ] && [ "$FS" != 'btrfs' ]; then
    echo "Invalid filesystem type: $FS (expected FAT, ext4, exFAT, or btrfs)" >&2
    exit 1
fi
if [ "$FS" == 'FAT' ]; then
    sfdisk --part-type "/dev/$RAWDEV" "$PARTNUM" "c"
    sleep 1
    mkfs.fat -- "/dev/$DEVICE"
elif [ "$FS" == 'ext4' ]; then
    sfdisk --part-type "/dev/$RAWDEV" "$PARTNUM" 83
    sleep 1
    mkfs.ext4 -F -- "/dev/$DEVICE"
elif [ "$FS" == 'exFAT' ]; then
    sfdisk --part-type "/dev/$RAWDEV" "$PARTNUM" 07
    sleep 1
    mkfs.exfat -- "/dev/$DEVICE"
elif [ "$FS" == 'btrfs' ]; then
    sfdisk --part-type "/dev/$RAWDEV" "$PARTNUM" 83
    sleep 1
    mkfs.btrfs -f -- "/dev/$DEVICE"
fi
