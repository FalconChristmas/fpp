#!/bin/sh

DEVICE="/dev/mmcblk0"

# Nuke all old partitions
parted -s ${DEVICE} rm 4 2> /dev/null
parted -s ${DEVICE} rm 3 2> /dev/null
parted -s ${DEVICE} rm 2 2> /dev/null
parted -s ${DEVICE} rm 1

# Create new FAT32
echo ",,c" | sfdisk ${DEVICE}

# Format the new partition
mkfs.vfat ${DEVICE}p1

# Show partition table
fdisk -l ${DEVICE}
