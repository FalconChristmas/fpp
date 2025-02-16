#!/bin/bash
FS=$1
DEVICE=$2
LEN=${#DEVICE}-1
PARTNUM=${DEVICE:$LEN}
RAWDEV=${DEVICE:0:$LEN}

if [[ $DEVICE == mmcb* ]] ; then
    LEN=${#DEVICE}-2
    RAWDEV=${DEVICE:0:$LEN}
fi

echo $RAWDEV   $PARTNUM
if [ $FS == 'FAT' ]; then
    sfdisk --part-type /dev/$RAWDEV $PARTNUM "c"
    sleep 1
    mkfs.fat /dev/$DEVICE
elif [ $FS == 'ext4' ]; then
    sfdisk --part-type /dev/$RAWDEV $PARTNUM 83
    sleep 1
    mkfs.ext4 -F /dev/$DEVICE
elif [ $FS == 'exFAT' ]; then
    sfdisk --part-type /dev/$RAWDEV $PARTNUM 07
    sleep 1
    mkfs.exfat /dev/$DEVICE
elif [ $FS == 'btrfs' ]; then
    sfdisk --part-type /dev/$RAWDEV $PARTNUM 83
    sleep 1
    mkfs.btrfs -f /dev/$DEVICE
fi
