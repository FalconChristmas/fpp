#!/bin/bash

device=$1
version_string=$2

if [ $USER != "root" ]; then
	echo "Must be root!"
	exit 1
fi

if [ -z "$1" ]; then
	echo "Error, need device with FPP SD card as first argument" >&2
	exit 1
fi

if [ ! -e $device ] && [ ! -e /dev/$device ]; then
	echo "Error, not a valid device as FPP SD card as first arg" >&2
	exit 1
fi

if [ -z "$version_string" ]; then
	echo "Error, no version string" >&2
	exit 1
fi

TMPDIR=$(mktemp -d)
CUR_DIR=$(pwd)

cleanup() {
	trap - EXIT INT
	if [ -n "$1" ]; then
		echo $* >&2
	fi

	cd $(dirs -p | tail -n 1) &> /dev/null
	dirs -c &> /dev/null

	rm -rf $TMPDIR
	exit 1
}

trap cleanup EXIT INT

set -x
set -o pipefail

#1. download latest noobs
#wget --content-disposition http://downloads.raspberrypi.org/NOOBS_latest || cleanup "Failed download"

#2. extract
unzip -q NOOBS_*.zip -d ${TMPDIR}/ || cleanup "Failed extraction"

#3. go to OS dir
pushd ${TMPDIR}/os || cleanup "Failed to cd to OS directory"

#4. copy raspbian to fpp
mv Raspbian FPP || cleanup "Failed to move Raspbian to FPP"

#5. edit os.json and change name/description fields
sed -i 's/"name": "[^"]*"/"name": "Falcon Player"/' FPP/os.json || \
	cleanup "Failed to modify name in os.json"
sed -i 's/"description": "[^"]*"/"description": "A lightweight, optimized, feature-rich sequence player designed to run on low-cost Single Board Computers."/' FPP/os.json || \
	cleanup "Failed to modify description in os.json"
if [ -n "$(which json_verify)" ]; then
	json_verify < FPP/os.json || \
		cleanup "Failed to update JSON for OS details, fix script"
else
	echo "Unable to check json, json_verify not found!" >&2
fi

#6. replace FPP.png
rm -f FPP/Raspbian*.png || cleanup "Failed to remove Raspbian logo file"
cp $CUR_DIR/FPP.png FPP/ || cleanup "Failed to copy in our FPP logo file"

#7. put in our slides
rm -rf FPP/slides_vga/ || cleanup "Failed to remove the original Rapbian slides"
cp -r $CUR_DIR/slides_vga/ FPP/ || cleanup "Failed to copy in our slides"

# Get some data for step 8
boot_mount_location=$(mount | grep $device | grep boot | awk '{print $3}')
my_boot_uncompressed=$(echo $(($(df $boot_mount_location | tail -n 1 | awk '{print $3}')*3/2/1000)))
my_boot_size=$(echo $(($(df $boot_mount_location | tail -n 1 | awk '{print $3}')*2/1000)))

root_mount_location=$(mount | grep $device | grep root | awk '{print $3}')
my_root_uncompressed=$(echo $(($(df $root_mount_location | tail -n 1 | awk '{print $3}')*5/4/1000)))
my_root_size=$(echo $(($(df $root_mount_location | tail -n 1 | awk '{print $3}')*2/1000)))

#8. edit partitions.json
#  i.  partition_size_nominal - numerical value with the size of the paritions in your custom OS version
boot_nominal=$(grep -A2 boot FPP/partitions.json  | grep partition_size_nominal | sed 's/.*": \([0-9]*\),/\1/')
root_nominal=$(grep -A2 root FPP/partitions.json  | grep partition_size_nominal | sed 's/.*": \([0-9]*\),/\1/')
if [ $my_boot_size -gt $boot_nominal ]; then
	sed -i "s/partition_size_nominal\": $boot_nominal/partition_size_nominal\": $my_boot_size/" FPP/partitions.json || \
		cleanup "Couldn't edit partition size for boot"
fi
if [ $my_root_size -gt $root_nominal ]; then
	sed -i "s/partition_size_nominal\": $root_nominal/partition_size_nominal\": $my_root_size/" FPP/partitions.json || \
		cleanup "Couldn't edit partition size for root"
fi

#  ii. uncompressed_tarball_size - numerical value with the size of your filesystem tarballs when uncompressed
boot_uncompressed=$(grep -A5 boot FPP/partitions.json  | grep uncompressed_tarball_size | sed 's/.*": \([0-9]*\)/\1/')
root_uncompressed=$(grep -A5 root FPP/partitions.json  | grep uncompressed_tarball_size | sed 's/.*": \([0-9]*\)/\1/')
if [ $my_boot_uncompressed -gt $boot_uncompressed ]; then
	sed -i "s/uncompressed_tarball_size\": $boot_uncompressed/uncompressed_tarball_size\": $my_boot_uncompressed/" FPP/partitions.json || \
		cleanup "Couldn't edit tarball size for boot"
fi
if [ $my_root_uncompressed -gt $root_uncompressed ]; then
	sed -i "s/uncompressed_tarball_size\": $root_uncompressed/uncompressed_tarball_size\": $my_root_uncompressed/" FPP/partitions.json || \
		cleanup "Couldn't edit tarball size for boot"
fi

if [ -n "$(which json_verify)" ]; then
	json_verify < FPP/partitions.json || \
		cleanup "Failed to update JSON for partition sizes, fix script"
else
	echo "Unable to check json, json_verify not found!" >&2
fi

#9. replace tar
#  i.  root tar: tar -cvpf <label>.tar /* --exclude=proc/* --exclude=sys/* --exclude=dev/pts/* --exclude-directory-with-dpkg-crap-found-by-looking-for:php5-cli
#      xz -9 -e <label>.tar
rm -f FPP/root.tar.xz
tar -cpf $(pwd)/FPP/root.tar -C $root_mount_location . --exclude=./root/.cpan --exclude=./root/.ccache --exclude=./var/cache/apt/archives/*.deb --exclude=./proc/* --exclude=./sys/* --exclude=./dev/pts/* --exclude=/etc/ssh/ssh_host*key* || cleanup "Failed to tar root"
xz -T $(grep -c ^processor /proc/cpuinfo) -9 -e FPP/root.tar || cleanup "Failed to compress root"

#  ii. boot tar: tar -cvpf <label>.tar . #at the root directory of the boot partition
#      xz -9 -e <label>.tar.
rm -f FPP/boot.tar.xz
tar -cpf $(pwd)/FPP/boot.tar -C $boot_mount_location . || cleanup "Failed to tar boot"
xz -T $(grep -c ^processor /proc/cpuinfo) -9 -e FPP/boot.tar || cleanup "Failed to compress boot"

#install w/o user intervention
sed -i 's/$/ silentinstall/' ../recovery.cmdline || cleanup "Failed to set silentinstall on cmdline"
rm -f FPP/flavours.json || cleanup "Failed to remove flavours.json"
rm -rf Data_Partition/

pushd ..
zip -r -9 ${CUR_DIR}/FPP-${version_string}-Pi.zip . || cleanup "Couldn't zip resulting data for image"
popd

popd || cleanup "Oddly not at root, what's going on?"

rm -rf ${TMPDIR}

# Clear our cleanup
trap - EXIT INT
