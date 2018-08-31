#!/bin/sh -e
#
# Copyright (c) 2014-2018 Robert Nelson <robertcnelson@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

if ! id | grep -q root; then
	echo "must be run as root"
	exit
fi

if [ ! -f /etc/ssh/ssh_host_ecdsa_key.pub ] ; then
	echo "Please wait a few more seconds as ssh keys are still being generated..."
	exit 1
fi

unset root_drive
root_drive="$(cat /proc/cmdline | sed 's/ /\n/g' | grep root=UUID= | awk -F 'root=' '{print $2}' || true)"
if [ ! "x${root_drive}" = "x" ] ; then
	root_drive="$(/sbin/findfs ${root_drive} || true)"
else
	root_drive="$(cat /proc/cmdline | sed 's/ /\n/g' | grep root= | awk -F 'root=' '{print $2}' || true)"
fi

if [ ! "x${root_drive}" = "x" ] ; then
	boot_drive="${root_drive%?}1"
else
	echo "Error: script halting, could detect drive..."
	exit 1
fi

single_partition () {
	echo "${drive}p1" > /resizerootfs
	conf_boot_startmb=${conf_boot_startmb:-"4"}
	sfdisk_fstype=${sfdisk_fstype:-"L"}
	if [ "x${sfdisk_fstype}" = "x0x83" ] ; then
		sfdisk_fstype="L"
	fi

	sfdisk_options="--force --no-reread --Linux --in-order --unit M"
	test_sfdisk=$(LC_ALL=C sfdisk --help | grep -m 1 -e "--in-order" || true)
	if [ "x${test_sfdisk}" = "x" ] ; then
		echo "sfdisk: 2.26.x or greater"
		sfdisk_options="--force --no-reread"
		conf_boot_startmb="${conf_boot_startmb}M"
	fi

	LC_ALL=C sfdisk ${sfdisk_options} ${drive} <<-__EOF__
		${conf_boot_startmb},,${sfdisk_fstype},*
	__EOF__
}

dual_partition () {
	echo "${drive}p2" > /resizerootfs
	conf_boot_startmb=${conf_boot_startmb:-"4"}
	conf_boot_endmb=${conf_boot_endmb:-"96"}
	sfdisk_fstype=${sfdisk_fstype:-"E"}

	sfdisk_options="--force --no-reread --Linux --in-order --unit M"
	test_sfdisk=$(LC_ALL=C sfdisk --help | grep -m 1 -e "--in-order" || true)
	if [ "x${test_sfdisk}" = "x" ] ; then
		echo "sfdisk: 2.26.x or greater"
		sfdisk_options="--force --no-reread"
		if [ "x${uboot_efi_mode}" = "xenable" ] ; then
			sfdisk_options="--force --no-reread --label gpt"
		fi
		conf_boot_startmb="${conf_boot_startmb}M"
		conf_boot_endmb="${conf_boot_endmb}M"
	fi

	LC_ALL=C sfdisk ${sfdisk_options} ${drive} <<-__EOF__
		${conf_boot_startmb},${conf_boot_endmb},${sfdisk_fstype},*
                100M,,,-
	__EOF__
}

expand_partition () {
	if [ -f /boot/SOC.sh ] ; then
		. /boot/SOC.sh
	fi

	if [ "x${boot_drive}" = "x/dev/mmcblk0p1" ] ; then
		drive="/dev/mmcblk0"
	elif [ "x${boot_drive}" = "x/dev/mmcblk1p1" ] ; then
		drive="/dev/mmcblk1"
	else
		echo "Error: script halting, could detect drive..."
		exit 1
	fi

	echo "Media: [${drive}]"

	if [ "x${boot_drive}" = "x${root_drive}" ] ; then
		single_partition
	else
		dual_partition
	fi
}

expand_partition
echo "reboot"
#
