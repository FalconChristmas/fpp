#!/bin/sh
###############################################################################
# Custom FPP SD to NOOBS image script initially based off Pi-Noobs-Converter.sh
# by mr-minifig on github:  https://github.com/mr-minifig/Pi-Noobs-Converter
###############################################################################
# Helper Functions

ShowUsage() {
	echo "USAGE: $0 FPP_IMAGE_FILE OUTPUT_DIR"
	exit 1
}

CleanupSource() {
	echo "========================================================================="
	echo "Cleaning up the source image before copying files..."

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

	sed -i 's/mmcblk0p${SRCROOT}/mmcblk0p5/g' ${SRCBOOTPATH}/cmdline.txt
	#cat ${SRCBOOTPATH}/cmdline.txt

	FILES="etc/resolv.conf root/.bash_history root/.nano_history home/pi/.bash_history home/pi/.nano_history home/pi/.lesshst"
	for FILE in ${FILES}
	do
		> ${SRCROOTPATH}/${FILE}
	done

	rm  -f ${SRCROOTPATH}/home/pi/THIS_IS_THE_MASTER_IMAGE 2> /dev/null
	rm  -f ${SRCROOTPATH}/var/lib/dhcp/dhclient*leases 2> /dev/null
	rm -rf ${SRCROOTPATH}/tmp/* 2> /dev/null
	rm -rf ${SRCROOTPATH}/tmp/.* 2> /dev/null
	rm -rf ${SRCROOTPATH}/home/pi/fpp* 2> /dev/null
	rm -rf ${SRCROOTPATH}/home/pi/.ssh/* 2> /dev/null
	rm -rf ${SRCROOTPATH}/etc/ssh/ssh_host*key* 2> /dev/null

	(cd ${SRCROOTPATH}/opt/fpp && git reset --hard)
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
OUTPUT_DIR=$2

echo "========================================================================="
echo "Creating tarballs for SD image $1"

# Source paritions on a booted FPP SD image
SRCBOOT="5"
SRCROOT="6"

SRCBOOTPATH="/tmp/FPP-Noobs-Packager/${SRCBOOT}"
SRCROOTPATH="/tmp/FPP-Noobs-Packager/${SRCROOT}"
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
echo "Building Tarballs..."
/bin/echo -n "Creating ${OUTPUT_DIR}/FPP-boot.tar: "
date
(cd ${SRCBOOTPATH} && tar -cpf ${OUTPUT_DIR}/FPP-boot.tar .)
/bin/echo -n "Finished: "
date

/bin/echo -n "Compressing ${OUTPUT_DIR}/FPP-boot.tar: "
date
xz -f -9 -e ${OUTPUT_DIR}/FPP-boot.tar
/bin/echo -n "Finished: "
date

/bin/echo -n "Creating ${OUTPUT_DIR}/FPP-root.tar: "
date
(cd ${SRCROOTPATH} && tar -cpf ${OUTPUT_DIR}/FPP-root.tar * --anchored --exclude=proc/* --exclude=sys/* --exclude=dev/pts/*)
/bin/echo -n "Finished: "
date

/bin/echo -n "Compressing ${OUTPUT_DIR}/FPP-root.tar: "
date
xz -f -9 -e ${OUTPUT_DIR}/FPP-root.tar
/bin/echo -n "Finished: "
date

###########################################################
echo "========================================================================="
echo "Unmounting source..."
umount ${SRCBOOTPATH}
umount ${SRCROOTPATH}

if [ ${IMGISDEVICE} -eq 0 ]
then
	kpartx -d /dev/loop0
	losetup -d /dev/loop0
fi

rmdir ${SRCBOOTPATH} ${SRCROOTPATH}

if [ ${IMGISDEVICE} -eq 1 ]
then
	echo "Image has been copied, you may remove the memory card anytime."
fi

###########################################################
echo "========================================================================="
/bin/echo -n "Finished: "
date

###########################################################
