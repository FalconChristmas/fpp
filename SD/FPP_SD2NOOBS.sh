#!/bin/sh
###############################################################################
# Custom FPP SD to NOOBS image script initially based off Pi-Noobs-Converter.sh
# by mr-minifig on github:  https://github.com/mr-minifig/Pi-Noobs-Converter
###############################################################################
# Helper Functions

ShowUsage() {
	echo "USAGE: $0 FPP_IMAGE_FILE FPP_VERSION"
	exit 1
}

CleanupSource() {
	echo "========================================================================="
	echo "Cleaning up the source image before calculating required size..."

	echo "/boot.bak"
	rm -f ${SRCROOTPATH}/boot.bak 2> /dev/null
	echo "/lib/modules.bak"
	rm -f ${SRCROOTPATH}/lib/modules.bak 2> /dev/null
	echo "/var/cache/apt/archives/*.deb"
	rm -f ${SRCROOTPATH}/var/cache/apt/archives/*.deb 2> /dev/null
	echo "/var/lib/logrotate/status"
	rm -f ${SRCROOTPATH}/var/lib/logrotate/status 2> /dev/null
	echo "/root/.cpan"
	rm -rf ${SRCROOTPATH}/root/.cpan 2> /dev/null
	echo "/root/.aptitude"
	rm -rf ${SRCROOTPATH}/root/.aptitude 2> /dev/null
}

FixupDestination() {
	echo "========================================================================="
	echo "Fixing up some files on the new image..."
#	echo ${VERSION} > ${DSTROOTPATH}/etc/fpp/rfs_version

	sed -i 's/mmcblk0p${SRCROOT}/mmcblk0p5/g' ${DSTBOOTPATH}/cmdline.txt
	#cat ${DSTBOOTPATH}/cmdline.txt

	FILES="etc/resolv.conf root/.bash_history root/.nano_history home/pi/.bash_history home/pi/.nano_history home/pi/.lesshst"
	for FILE in ${FILES}
	do
		> ${DSTROOTPATH}/${FILE}
	done

	rm  -f ${DSTROOTPATH}/var/lib/dhcp/dhclient*leases 2> /dev/null
	rm -rf ${DSTROOTPATH}/tmp/* 2> /dev/null
	rm -rf ${DSTROOTPATH}/tmp/.* 2> /dev/null
	rm -rf ${DSTROOTPATH}/home/pi/fpp* 2> /dev/null
	rm -rf ${DSTROOTPATH}/home/pi/.ssh/* 2> /dev/null

	(cd ${DSTROOTPATH}/opt/fpp && git reset --hard)
}

###############################################################################
# Main Script
command -v losetup >/dev/null 2>&1 || { echo "I require losetup but it's not installed. Aborting." >&2; exit 1; }
command -v kpartx >/dev/null 2>&1 || { echo "I require kpartx but it's not installed. Aborting." >&2; exit 1; }
command -v parted >/dev/null 2>&1 || { echo "I require kpartx but it's not installed. Aborting." >&2; exit 1; }

if [ -z "$1" -o -z "$2" ]
then
	ShowUsage
fi

ORIG_IMG=$1
VERSION=$2
NEW_IMG="FPP-${VERSION}-NOOBS.img"

echo "========================================================================="
echo "Re-packaging SD image $1 to ${NEW_IMG} as FPP version ${VERSION}"

# Source paritions on a booted FPP SD image
SRCBOOT="5"
SRCROOT="6"
# Destination partitions on a Noobs image
DSTBOOT="1"
DSTROOT="5"

SRCBOOTPATH="/tmp/Noobs-Converter/Orig/${SRCBOOT}"
SRCROOTPATH="/tmp/Noobs-Converter/Orig/${SRCROOT}"
DSTBOOTPATH="/tmp/Noobs-Converter/New/${DSTBOOT}"
DSTROOTPATH="/tmp/Noobs-Converter/New/${DSTROOT}"
IMGISDEVICE=0

echo "========================================================================="
echo "Creating temporary working directory..."
mkdir -p ${SRCBOOTPATH} ${SRCROOTPATH}

echo "========================================================================="
echo "Mounting Original Image..."
IMGHEAD=$(echo ${ORIG_IMG} | cut -c1-5)
if [ "x${IMGHEAD}" = "x/dev/" ]
then
	# Try readng directly from the SD card if /dev/* path given
	IMGISDEVICE=1
	mount ${ORIG_IMG}${SRCBOOT} ${SRCBOOTPATH}
	mount ${ORIG_IMG}${SRCROOT} ${SRCROOTPATH}
else
	losetup /dev/loop0 $ORIG_IMG
	kpartx -av /dev/loop0
	mount /dev/mapper/loop0p${SRCBOOT} ${SRCBOOTPATH}
	mount /dev/mapper/loop0p${SRCROOT} ${SRCROOTPATH}
fi

###########################################################
# Call our function to cleanup the source dir
CleanupSource

###########################################################
echo "========================================================================="
echo "Creating New Empty NOOBS Image..."
size=0
if [ ${IMGISDEVICE} -eq 1 ]
then
	size=$(($(df|grep ${ORIG_IMG}${SRCBOOT}|tr -s ' '|cut -d ' ' -f3) + $(df|grep ${ORIG_IMG}${SRCROOT}|tr -s ' '|cut -d ' ' -f3) + 200000))
else
	size=$(($(df|grep loop0p${SRCBOOT}|tr -s ' '|cut -d ' ' -f3) + $(df|grep loop0p${SRCROOT}|tr -s ' '|cut -d ' ' -f3) + 200000))
fi
echo "New image file size is: $size KiB"

mkdir -p ${DSTBOOTPATH} ${DSTROOTPATH}

if [ -f "${NEW_IMG}" ]
then
	rm ${NEW_IMG}
fi

dd bs=1024 if=/dev/zero of=${NEW_IMG} count=0 seek=$size
losetup /dev/loop1 ${NEW_IMG}
kpartx -av /dev/loop1

###########################################################
echo "========================================================================="
echo "Building partition Table..."
parted -s  /dev/loop1 mklabel msdos
parted -s  /dev/loop1 mkpart  primary  fat16 4194kb 62.9MB
parted -s  /dev/loop1 mkpart  extended 62.9MB 100%
parted -s  /dev/loop1 mkpart  logical  62.9MB 100%
kpartx -av /dev/loop1

echo "Sleeping a few seconds to let the devices be created..."
sleep 3
###########################################################
echo "========================================================================="
echo "Formatting partitions..."
mkfs.vfat /dev/mapper/loop1p${DSTBOOT}
mkfs.ext4 /dev/mapper/loop1p${DSTROOT}

###########################################################
echo "========================================================================="
echo "Mounting New Image..."
mount /dev/mapper/loop1p${DSTBOOT} ${DSTBOOTPATH}
mount /dev/mapper/loop1p${DSTROOT} ${DSTROOTPATH}

###########################################################
echo "========================================================================="
echo "Transfering Data..."
rsync -avz --stats --progress ${SRCBOOTPATH}/ ${DSTBOOTPATH}/
rsync -avz --stats --progress ${SRCROOTPATH}/ ${DSTROOTPATH}/

###########################################################
# Call our function to fix/setup some files in the new image
FixupDestination

###########################################################
echo "========================================================================="
echo "Tidying up..."
umount ${SRCBOOTPATH}
umount ${SRCROOTPATH}

if [ ${IMGISDEVICE} -eq 0 ]
then
	kpartx -d /dev/loop0
	losetup -d /dev/loop0
fi

umount ${DSTBOOTPATH}
umount ${DSTROOTPATH}
kpartx -d /dev/loop1
losetup -d /dev/loop1

rmdir ${SRCBOOTPATH} ${SRCROOTPATH}
rmdir ${DSTBOOTPATH} ${DSTROOTPATH}

sync

if [ ${IMGISDEVICE} -eq 1 ]
then
	echo "Image has been copied, you may remove the memory card anytime."
fi
###########################################################
echo "========================================================================="
echo "Compressing ${NEW_IMG}"
echo "This may take some time!"
echo -n "Compression started at: "
date

xz --verbose -9 -e $NEW_IMG 

echo -n "Compression ended at  : "
date

###########################################################
