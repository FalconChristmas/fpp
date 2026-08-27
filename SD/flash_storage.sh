#!/bin/bash
#
# flash_storage.sh -- copy the running FPP install onto another storage device.
#
# Replaces the per-platform flash scripts with a single flow.  Every platform runs
# the same seven phases:
#
#     preflight -> partition -> format -> mount -> copy -> fixup -> verify
#
# Only the phases marked "profile" below differ per platform, and they differ as
# lookup tables (partition layout, labels, which file carries root=), not as logic.
#
# The invariant that matters, and that the old Pi path could not state: the
# destination boot filesystem is populated from a *named* source directory that
# preflight has proven is mounted, and verify refuses to report success unless the
# destination carries the files that platform needs in order to boot.  The previous
# Pi path inferred the boot partition from the live mount table, so an unmounted
# /boot/firmware silently dropped it from the copy and still printed "complete".
#
# Usage:
#   flash_storage.sh [options] <device>
#
#     <device>        target disk, with or without /dev (sda, nvme0n1, mmcblk0)
#
#     --fresh         (default) a clean install: no media, fresh host identity.
#     --clone         the same, plus this player's media, sequences, playlists and
#                     config.
#
#   Those two differ only in content.  Host identity -- ssh host keys and
#   machine-id -- is reset either way, because the copy lands on a different physical
#   device that may well run alongside this one, and duplicates of those collide
#   (systemd derives the DHCP DUID from machine-id, so two boxes can fight over a
#   lease).  This costs nothing in FPP terms: a player's identity for multiSync and
#   xLights comes from the CPU serial via scripts/get_uuid, not from machine-id, so a
#   flashed copy keeps its own identity regardless.
#
#     --media / --no-media
#                     content, if you want it independently of the above.
#     --keep-identity Carry the ssh host keys and machine-id across as well.  Only
#                     for moving an install onto a bigger disk and retiring the
#                     original -- never when both will run.
#     --scrub-identity  (default) reset them.
#
#     --root-size SZ  size of the root partition as an sfdisk size (e.g. 8G);
#                     default is the rest of the disk
#     --btrfs         btrfs root filesystem (BeagleBone only)
#     --reboot        power down when finished (the eMMC flow expects this)
#     --no-reboot     (default) leave the machine running when finished
#     --dry-run       run preflight and print the plan, then stop
#     -y | --yes      do not prompt
#

BINDIR=$(cd "$(dirname "$0")" && pwd)

# Sourced before `set -e`: scripts/common uses `[ -f x ] && y` at top level, which
# returns non-zero whenever the optional file is absent (e.g. /etc/fpp/rfs_version
# on a freshly built image) and would abort us before we started.
. "${BINDIR}/../scripts/common"

# From here on any unchecked failure is fatal.  A flash that half-worked and kept
# going is the failure mode this whole script exists to remove.
set -eo pipefail

PGM=$(basename "$0")

COPY_MEDIA="n"
KEEP_IDENTITY="n"
ROOT_SIZE=""
ROOTFS_TYPE="ext4"
DOREBOOT="n"
DRYRUN="n"
ASSUME_YES="n"
DEVICE=""

# Working mount points.  Deliberately not /mnt -- the old Pi path mounted the
# destination there, left it mounted, and then aborted on the next run because
# umount /mnt hit EBUSY on the nested boot mount.
DSTROOT="/tmp/fpp-flash/root"

die() {
    echo "" >&2
    echo "ERROR: $*" >&2
    echo "" >&2
    exit 1
}

info() { echo "$*"; }

# Phase gating.
#
# `set -e` is not sufficient here and neither is `phase || die`.  A failed
# arithmetic (or similar) expansion makes bash unwind EVERY function frame --
# including any checker function -- and resume at the next top-level command, with
# the aborted function reporting success.  Verified on bash 5.2.
#
# So each phase records its own name as its last action, and the check is a
# top-level command.  A phase that did not reach its final line cannot be mistaken
# for one that did, however it died.
PHASE_COMPLETED=""
phase_ok() {
    [ "${PHASE_COMPLETED}" = "$1" ] || die "$1 did not run to completion."
    PHASE_COMPLETED=""
}

banner() {
    echo ""
    echo "---------------------------------------"
    echo "$*"
    echo ""
}

# mkfs.ext4 defaults to lazy_itable_init=1: it leaves the inode tables unzeroed and
# hands that work to the kernel's ext4lazyinit thread on first mount.  On a big root
# filesystem that means the freshly flashed player spends its first several minutes
# writing hard with no process to blame it on -- measured on a 250GB root: ~1.35GB
# written over ~8 minutes of an otherwise idle box, invisible in top and in
# /proc/<pid>/io because a kernel thread owns it.  A customer unboxing a player sees
# a disk light that will not stop.
#
# Flashing is already a watch-the-progress operation, so do the zeroing here where
# it is expected and accounted for, and hand over a filesystem that is finished.
# lazy_journal_init is 0 by default; state it so a future mke2fs.conf cannot quietly
# turn it on.
mkfs_ext4() {
    mkfs.ext4 -F -E lazy_itable_init=0,lazy_journal_init=0 "$@"
}

usage() {
    # to the first truly blank line, i.e. the end of the header comment block
    sed -n '/^# Usage:/,/^$/p' "$0" | sed 's/^# \?//'
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --clone)          COPY_MEDIA="y" ;;
        --fresh)          COPY_MEDIA="n"; KEEP_IDENTITY="n" ;;
        --media)          COPY_MEDIA="y" ;;
        --no-media)       COPY_MEDIA="n" ;;
        --keep-identity)  KEEP_IDENTITY="y" ;;
        --scrub-identity) KEEP_IDENTITY="n" ;;
        --root-size) ROOT_SIZE="$2"; shift ;;
        --btrfs)     ROOTFS_TYPE="btrfs" ;;
        --no-reboot) DOREBOOT="n" ;;
        --reboot)    DOREBOOT="y" ;;
        --dry-run)   DRYRUN="y" ;;
        -y|--yes)    ASSUME_YES="y" ;;
        -h|--help)   usage ;;
        -*)          die "unknown option: $1" ;;
        *)           DEVICE="$1" ;;
    esac
    shift
done

[ "$(id -u)" = "0" ] || die "${PGM} must be run as root."

# Armed here, not just before the destructive phases: an unattended factory flash
# that dies in preflight (eMMC too small, boot filesystem missing) has to signal just
# as loudly as one that dies halfway through the copy.
FLASH_OUTCOME="failed"
trap on_exit EXIT


#############################################################################
# Platform profiles
#
# Each profile fills in the same set of variables and functions.  Shared fields:
#
#   PROFILE            short name, for messages
#   SRC_BOOT_DIR       directory on THIS system holding the boot configuration
#   SRC_BOOT_SEPARATE  y if SRC_BOOT_DIR is its own filesystem that must be mounted
#   DST_BOOT_DIR       where that filesystem gets mounted on the destination
#   VERIFY_BOOT_FILES  files that must exist on the destination boot fs afterwards
#   VERIFY_ROOT_FILES  files that must exist on the destination root fs afterwards
#############################################################################

detect_profile() {
    case "${FPPPLATFORM}" in
        "Raspberry Pi")
            PROFILE="pi"
            ;;
        # Both strings are in use: /etc/fpp/platform reads "BeagleBone Black" on the
        # AM335x and "BeagleBone 64" on the AM62x boards (PocketBeagle2).  Matching
        # only the first is why this refused to run on a PocketBeagle2 at all.
        "BeagleBone Black"|"BeagleBone 64")
            if [ "$(uname -m)" = "aarch64" ]; then
                PROFILE="bb64"
            else
                PROFILE="bbb"
            fi
            ;;
        *)
            die "${PGM} does not support platform '${FPPPLATFORM}'."
            ;;
    esac
}

#---------------------------------------------------------------------------
# Raspberry Pi: FAT firmware partition + ext4 root, boot config in config.txt
# and cmdline.txt, root selected by device path in cmdline.txt and by label in
# fstab.  USB and NVMe targets additionally need bootloader coaxing.
#---------------------------------------------------------------------------
profile_pi_init() {
    SRC_BOOT_DIR="/boot/firmware"
    SRC_BOOT_SEPARATE="y"
    DST_BOOT_DIR="/boot/firmware"
    # config.txt/cmdline.txt alone are not enough -- a stale or half-copied FAT can
    # carry those and still not boot.  Require a kernel and the overlays the cape
    # code depends on.
    VERIFY_BOOT_FILES="config.txt cmdline.txt overlays"
    VERIFY_BOOT_GLOBS="kernel*.img|vmlinuz*"
    VERIFY_ROOT_FILES="etc/fstab opt/fpp/www/index.php"

    # Labels have to be unique across every disk the Pi can see: fstab mounts by
    # LABEL, so two disks claiming "boot" is an ambiguity that surfaces as the wrong
    # filesystem being mounted.  The first three cases are the historical names, kept
    # so an existing install keeps booting; anything else is derived.  FAT labels cap
    # at 11 characters and ext4 at 16.
    label_for() {
        local base
        case "${DEVICE}" in
            mmcblk0) base=""     ;;
            nvme0n1) base="nvme" ;;
            *)       base="${DEVICE//[^a-zA-Z0-9]/}" ;;
        esac
        local l="${1}${base}"
        echo "${l:0:$2}"
    }
    BOOT_LABEL=$(label_for boot 11)
    ROOT_LABEL=$(label_for rootfs 16)

    case "${DEVICE}" in
        sd[a-z]|nvme[0-9]n[0-9]|mmcblk[0-9]) ;;
        *) die "unsupported Raspberry Pi target '${DEVICE}'." ;;
    esac
}

profile_pi_preflight() {
    # A Pi 4 will not boot from USB or NVMe at all without a current bootloader, so
    # this has to happen before the copy is worth making.  Applying an update stages
    # recovery.bin and needs a reboot to take effect; stop and say so rather than
    # producing a drive the Pi cannot reach.
    banner "Checking for a bootloader update (needed to boot from USB/NVMe)"
    rpi-eeprom-update -a || info "  (eeprom update check failed; continuing)"

    if [ -f "${SRC_BOOT_DIR}/recovery.bin" ]; then
        echo ""
        echo "******  An EEPROM update was staged.  Reboot to apply it, then flash again.  ******"
        echo ""
        exit 0
    fi
}

profile_pi_partition() {
    # Match the source boot partition's size rather than hardcoding, so images that
    # change it stay self-consistent.
    local bootbytes bootmb
    bootbytes=$(blockdev --getsize64 "${SRC_BOOT_DEV}") \
        || die "cannot read the size of ${SRC_BOOT_DEV}."
    case "${bootbytes}" in
        ''|*[!0-9]*) die "unexpected size '${bootbytes}' for ${SRC_BOOT_DEV}." ;;
    esac
    bootmb=$(( bootbytes / 1048576 ))
    [ "${bootmb}" -ge 64 ] || die "source boot partition is only ${bootmb}MB -- refusing to guess."

    info "  boot partition: ${bootmb}MB   root partition: ${ROOT_SIZE:-rest of disk}"
    DST_NPARTS=2

    sfdisk --force "/dev/${DEVICE}" <<-EOF
	label: dos
	,${bootmb}M,c,*
	,${ROOT_SIZE},83,
	EOF
}

profile_pi_format() {
    mkfs.vfat -F 32 -n "${BOOT_LABEL}" "${DSTPART}1"
    mkfs_ext4 -L "${ROOT_LABEL}" "${DSTPART}2"
}

profile_pi_mount() {
    mount "${DSTPART}2" "${DSTROOT}"
    mkdir -p "${DSTROOT}${DST_BOOT_DIR}"
    mount "${DSTPART}1" "${DSTROOT}${DST_BOOT_DIR}"
}

profile_pi_fixup() {
    local cmdline="${DSTROOT}${DST_BOOT_DIR}/cmdline.txt"
    local config="${DSTROOT}${DST_BOOT_DIR}/config.txt"

    [ -f "${cmdline}" ] || die "no cmdline.txt on the destination boot filesystem."
    [ -f "${config}" ]  || die "no config.txt on the destination boot filesystem."

    # ${DSTPART}2, not /dev/${DEVICE}p2 -- sda's second partition is sda2, not sdap2.
    sed -i "s|root=/dev/[a-zA-Z0-9]* |root=${DSTPART}2 |g;
            s|root=LABEL=[a-zA-Z0-9]* |root=${DSTPART}2 |g;
            s|root=PARTUUID=[a-zA-Z0-9-]* |root=${DSTPART}2 |g" "${cmdline}"

    write_fstab <<-EOF
	proc            /proc           proc    defaults          0       0
	LABEL=${BOOT_LABEL} ${DST_BOOT_DIR}  vfat    defaults          0       2
	LABEL=${ROOT_LABEL} /               ext4    defaults,noatime  0       1
	EOF

    # program_usb_boot_mode enables USB boot on a Pi 4 / CM4 by blowing a one-way OTP
    # fuse.  A Pi 5 boots from USB out of the box and has no such fuse, so the line is
    # meaningless there -- and it is the only content difference between a working SD
    # and a USB copy that would not boot, so do not write it where it does nothing.
    # Note this lands in the DESTINATION's config.txt, so it is acted on by whatever
    # Pi that drive is eventually booted in.
    sed -i "/^program_usb_boot_mode=/d" "${config}"
    sed -i "/^usb_max_current_enabled=/d" "${config}"
    case "${DEVICE}" in
        sd[a-z])
            if grep -qE "Raspberry Pi (5|Compute Module 5)" /proc/device-tree/model 2>/dev/null; then
                info "  Pi 5: skipping program_usb_boot_mode (not applicable)"
            else
                echo "program_usb_boot_mode=1" >> "${config}"
            fi
            # A Pi 5 caps total USB current at 600mA unless it detects a 5A supply,
            # which is not enough for many SSDs and enclosures -- they brown out and
            # reset mid-transfer, or never enumerate at all.  A system whose root
            # filesystem IS the USB device cannot afford that, so lift the cap to
            # 1.6A on any USB target.
            echo "usb_max_current_enabled=1" >> "${config}"
            ;;
    esac
}

profile_pi_finish() {
    if [ "${DEVICE}" = "nvme0n1" ]; then
        banner "Setting EEPROM BOOT_ORDER for NVMe"
        printf "[all]\nBOOT_ORDER=0xf61\n" > /tmp/fpp-boot.conf
        rpi-eeprom-config --apply /tmp/fpp-boot.conf || \
            info "  WARNING: could not set BOOT_ORDER; the Pi may not boot from NVMe."
        rm -f /tmp/fpp-boot.conf
    fi
}

#---------------------------------------------------------------------------
# BeagleBone Black (AM335x).  Boots from /boot on the root filesystem via
# uEnv.txt; MLO/u-boot live in raw sectors and are written by the bootloader
# installer, not by the partition copy.
#---------------------------------------------------------------------------
profile_bbb_init() {
    SRC_BOOT_DIR="/boot"
    SRC_BOOT_SEPARATE="n"
    DST_BOOT_DIR=""
    VERIFY_BOOT_FILES=""
    VERIFY_BOOT_GLOBS=""
    VERIFY_ROOT_FILES="etc/fstab boot/uEnv.txt opt/fpp/www/index.php"

    [ -n "${DEVICE}" ] || DEVICE="mmcblk1"
    [ "${DEVICE}" = "mmcblk1" ] || die "unsupported BeagleBone target '${DEVICE}' (expected mmcblk1)."
}

# Which of the three eMMC layouts to use.  Matches the original BBB-FlashMMC.sh
# exactly: a small eMMC (the classic 4GB BBB part is ~7.5M sectors) cannot afford a
# separate boot and swap partition, so everything goes on one ext4.
bbb_layout() {
    local devsz
    if [ "${ROOTFS_TYPE}" = "btrfs" ]; then
        BBB_LAYOUT="btrfs"
        return
    fi
    devsz=$(blockdev --getsz "/dev/${DEVICE}") || die "cannot read the size of /dev/${DEVICE}."
    case "${devsz}" in ''|*[!0-9]*) die "unexpected size '${devsz}' for /dev/${DEVICE}." ;; esac
    if [ -z "${ROOT_SIZE}" ] && [ "${devsz}" -gt 9000000 ]; then
        BBB_LAYOUT="three"
    else
        BBB_LAYOUT="single"
    fi
}

profile_bbb_partition() {
    bbb_layout
    info "  eMMC layout: ${BBB_LAYOUT}"
    case "${BBB_LAYOUT}" in
        btrfs)
            sfdisk --force "/dev/${DEVICE}" <<-EOF
			4M,${ROOT_SIZE},,-
			EOF
            ;;
        three)
            sfdisk --force "/dev/${DEVICE}" <<-EOF
			4M,36M,c,*
			40M,512M,82,*
			542M,${ROOT_SIZE},,-
			EOF
            ;;
        single)
            sfdisk --force "/dev/${DEVICE}" <<-EOF
			4M,${ROOT_SIZE},,-
			EOF
            ;;
    esac
    case "${BBB_LAYOUT}" in
        btrfs)  DST_NPARTS=1 ;;
        three)  DST_NPARTS=3 ;;
        single) DST_NPARTS=1 ;;
    esac
}

profile_bbb_format() {
    case "${BBB_LAYOUT}" in
        btrfs)
            mkfs.btrfs -f "${DSTPART}1" -L rootfs
            ;;
        three)
            mkfs.vfat -F 16 "${DSTPART}1" -n boot
            mkswap "${DSTPART}2" -L swap
            mkfs_ext4 -O ^metadata_csum,^64bit "${DSTPART}3" -L rootfs
            ;;
        single)
            mkfs_ext4 -O ^metadata_csum,^64bit "${DSTPART}1" -L rootfs
            ;;
    esac
}

profile_bbb_mount() {
    case "${BBB_LAYOUT}" in
        btrfs)
            # zstd:1, not the default level 3.  Measured on an AM335x copying ~814MB
            # of real binaries and libraries: level 3 took 273s for 347MB, level 1
            # took 134s for 358MB.  Half the time for 3% more space, on a single core
            # that is already the bottleneck.  Negative levels are worse on both
            # counts here -- zstd:-3 saved 11s over level 1 and cost 73MB.
            #
            # Plain `compress`, NOT compress-force, because /boot below must stay
            # readable by u-boot and compress-force overrides the NOCOMPRESS flag.
            # The space cost is small: `compress` only skips what btrfs judges
            # incompressible, which for a rootfs of binaries is very little.
            mount -t btrfs -o noatime,nodiratime,compress=zstd:1 "${DSTPART}1" "${DSTROOT}"

            # u-boot reads /boot itself on this layout, and its btrfs zstd path does
            # not work: it loads a compressed uEnv.txt, reports
            #
            #   ZSTD_decompressStream error 7
            #
            # hands back garbage, and then cannot see uname_r even though the file
            # plainly contains it -- so it scans for partitions that do not exist and
            # falls through to TFTP.  Marking these NOCOMPRESS *before* the copy keeps
            # everything u-boot reads stored raw; the flag is inherited by the files
            # rsync then creates inside them.
            #
            # Both directories, not just /boot: uEnv.txt points u-boot at
            # /lib/firmware/fpp-*-overlay.dtb, which is NOT under /boot.  Marking only
            # /boot got the kernel loading and then died on the device tree with
            # FDT_ERR_BADMAGIC / FDT_ERR_BADSTRUCTURE, which is what a mis-decompressed
            # dtb looks like.
            # Resolved through symlinks as they exist on the SOURCE, because the copy
            # reproduces that layout: /lib is a symlink to usr/lib on a usrmerge
            # system, so pre-creating "lib/firmware" just gets replaced by the symlink
            # and the real files land in usr/lib/firmware uncovered by the flag.
            local d real
            for d in /boot /lib/firmware; do
                real=$(readlink -f "${d}" 2>/dev/null || echo "${d}")
                mkdir -p "${DSTROOT}${real}"
                if chattr +m "${DSTROOT}${real}" 2>/dev/null; then
                    info "  ${real}: stored uncompressed for u-boot"
                else
                    info "  WARNING: could not mark ${real} NOCOMPRESS; u-boot may not read it"
                fi
            done
            ;;
        three)  mount -t ext4 -o noatime,nodiratime "${DSTPART}3" "${DSTROOT}" ;;
        single) mount -t ext4 -o noatime,nodiratime "${DSTPART}1" "${DSTROOT}" ;;
    esac
}

profile_bbb_fixup() {
    local uenv="${DSTROOT}/boot/uEnv.txt"

    [ -f "${uenv}" ] || die "no /boot/uEnv.txt on the destination."

    sed -i '/^mmcpart=/d;/^rootfstype=btrfs/d;/^mmcrootfstype=btrfs/d' "${uenv}"
    # An auto-flash boot leaves its own hook behind; it must not run on the copy.
    sed -i '/.*AutoFlash\.sh/d' "${uenv}"
    rm -f "${DSTROOT}"/boot/BBB-*

    ( cd "${DSTROOT}/boot" && rm -f boot && ln -sf . boot )

    case "${BBB_LAYOUT}" in
        btrfs)
            # One partition: u-boot 2022.04 reads btrfs (including zstd), so the 96MB
            # ext4 /boot this layout used to carry earns nothing -- and on a 3.7GB
            # eMMC that is real space.  It also removes the overlay staging that
            # layout needed: with /boot and /lib/firmware on the same filesystem,
            # u-boot resolves the paths uEnv.txt names directly.
            #
            # "btrfs rootwait fixrtc", not bare "btrfs".  u-boot's mmcrootfstype
            # carries the root flags along with the filesystem name -- its default is
            # "ext4 rootwait fixrtc" -- so overriding it with just the name drops
            # rootwait, and the kernel then panics before the eMMC has enumerated:
            #
            #   VFS: Cannot open root device "/dev/mmcblk1p2" ... error -6
            #   Kernel panic - not syncing: VFS: Unable to mount root fs
            #
            # The SDHCI controllers come up around 3.80s and the card lands ~50ms
            # later; without rootwait the kernel gives up at 3.83s.
            {
                echo ""
                echo "mmcrootfstype=btrfs rootwait fixrtc"
                echo "rootfstype=btrfs"
                echo "mmcpart=1"
                echo ""
            } >> "${uenv}"
            # plain `compress`, not compress-force: nearly everything written after
            # the flash is already-compressed media (mp3, fseq, video), and forcing
            # zstd over it burns CPU for nothing.
            write_fstab <<-EOF
			${DSTPART}1  /  btrfs  noatime,nodiratime,compress=zstd  0  1
			debugfs  /sys/kernel/debug  debugfs  defaults  0  0
			EOF
            ;;
        three)
            write_fstab <<-EOF
			${DSTPART}3  /               ext4  defaults,noatime,nodiratime  0  1
			${DSTPART}1  /boot/firmware  vfat  user,uid=1000,gid=1000,defaults  0  2
			${DSTPART}2  none            swap  sw  0  0
			debugfs  /sys/kernel/debug  debugfs  defaults  0  0
			EOF
            ;;
        single)
            write_fstab <<-EOF
			${DSTPART}1  /  ext4  defaults,noatime,nodiratime  0  1
			debugfs  /sys/kernel/debug  debugfs  defaults  0  0
			EOF
            ;;
    esac
}

profile_bbb_preflight() {
    [ "${ROOTFS_TYPE}" = "btrfs" ] || return 0

    # A btrfs root is only mountable if the running kernel can do it unaided: these
    # images ship no initramfs, so a btrfs built as a module is never available at
    # mount-root time.  The flash itself succeeds and verifies -- the copy on disk is
    # perfectly good -- and then the board hangs at boot with no explanation, which is
    # the worst possible outcome for something flashed before shipping.
    local kcfg="/boot/config-$(uname -r)"
    if ls /boot/initrd* >/dev/null 2>&1; then
        return 0
    fi
    if [ ! -f "${kcfg}" ]; then
        info "  WARNING: no ${kcfg}, cannot confirm this kernel can mount a btrfs root"
        return 0
    fi
    grep -q "^CONFIG_BTRFS_FS=y" "${kcfg}" || die \
"this kernel cannot mount a btrfs root filesystem, so the flash would produce a
 board that hangs at boot.

     $(grep -E '^CONFIG_BTRFS_FS[=_]' "${kcfg}" | head -1 || echo 'CONFIG_BTRFS_FS is not set')
     initramfs: none

 btrfs is a module here and there is no initramfs to load it from before the root
 filesystem is mounted.  Either build it in (CONFIG_BTRFS_FS=y) or flash without
 --btrfs."
}

# Run after verify, not before partitioning as the old script did.  It dd's MLO and
# u-boot into raw sectors of *every* MMC device present -- the booted SD card
# included -- so a flash that was going to fail anyway should not have touched them.
# The sectors it writes (128K-1920K) sit below the first partition in every layout
# here, so ordering against mkfs does not matter.
profile_bbb_finish() {
    if [ -f /opt/fpp/bin.bbb/bootloader/install.sh ]; then
        banner "Installing the bootloader"
        /opt/fpp/bin.bbb/bootloader/install.sh
    fi
}

#---------------------------------------------------------------------------
# BeagleBone 64 / PocketBeagle2 (AM62x).  extlinux on a FAT at /boot/firmware.
#---------------------------------------------------------------------------
profile_bb64_init() {
    SRC_BOOT_DIR="/boot/firmware"
    SRC_BOOT_SEPARATE="y"
    DST_BOOT_DIR="/boot/firmware"
    VERIFY_BOOT_FILES="extlinux/extlinux.conf"
    VERIFY_BOOT_GLOBS="Image*|vmlinuz*"
    VERIFY_ROOT_FILES="etc/fstab opt/fpp/www/index.php"

    [ -n "${DEVICE}" ] || DEVICE="mmcblk0"
    [ "${DEVICE}" = "mmcblk0" ] || die "unsupported BeagleBone 64 target '${DEVICE}' (expected mmcblk0)."
}

profile_bb64_partition() {
    DST_NPARTS=3
    sfdisk --force "/dev/${DEVICE}" <<-EOF
	1M,256M,c,*
	257M,1024M,82,*
	1281M,${ROOT_SIZE},,-
	EOF
}

profile_bb64_format() {
    mkfs.vfat -F 16 "${DSTPART}1" -n boot
    mkswap "${DSTPART}2" -L swap
    mkfs_ext4 -O ^metadata_csum,^64bit "${DSTPART}3" -L rootfs
}

profile_bb64_mount() {
    mount -t ext4 -o noatime,nodiratime "${DSTPART}3" "${DSTROOT}"
    mkdir -p "${DSTROOT}${DST_BOOT_DIR}"
    mount -t vfat -o noatime,nodiratime "${DSTPART}1" "${DSTROOT}${DST_BOOT_DIR}"
}

profile_bb64_fixup() {
    local extlinux="${DSTROOT}${DST_BOOT_DIR}/extlinux/extlinux.conf"

    [ -f "${extlinux}" ] || die "no extlinux/extlinux.conf on the destination boot filesystem."

    sed -i "s|root=/dev/[a-zA-Z0-9]*\([0-9]\) |root=/dev/${DEVICE}p3 |g;
            s|resume=/dev/[a-zA-Z0-9]*\([0-9]\) |resume=/dev/${DEVICE}p2 |g" "${extlinux}"
    # A flash booted from the auto-flash entry defaults to that label; the copy
    # must come up normally.
    sed -i "s|default flashEMMC|default microSD|g" "${extlinux}"
    rm -f "${DSTROOT}${DST_BOOT_DIR}/fpp_expand_rootfs"

    write_fstab <<-EOF
	/dev/${DEVICE}p3  /               ext4  defaults,noatime,nodiratime,errors=remount-ro  0  1
	/dev/${DEVICE}p2  none            swap  sw  0  0
	/dev/${DEVICE}p1  /boot/firmware  vfat  user,uid=1000,gid=1000,defaults  0  2
	debugfs  /sys/kernel/debug  debugfs  mode=755,uid=root,gid=gpio,defaults  0  0
	EOF
}

# The PocketBeagle2 Industrial is the only board this profile ever flashes: mmcblk0
# is the eMMC, and only the industrial has it populated.  Some of those boards leave
# the factory with a blank or half-written identity EEPROM, which makes u-boot bring
# them up as the 512MB base board -- so the EEPROM gets checked, and repaired if it
# can be, before anything is written to the eMMC.
#
# A board with no EEPROM at all cannot be repaired, and check_pb2_eeprom.sh exits
# non-zero for it.  Dying here is what the factory flow needs: preflight runs under
# the same EXIT trap as every other phase, so the abort ends with all the user LEDs
# blinking rather than with a board that looks finished.
profile_bb64_preflight() {
    local check="${BINDIR}/../capes/drivers/bb64/check_pb2_eeprom.sh"

    if [ ! -f "${check}" ]; then
        info "  ${check} not found; skipping the EEPROM check"
        return 0
    fi

    /bin/bash "${check}" || die \
"the on-board EEPROM is not usable.  See the message above.

 Flashing would produce a board that misreports its variant, and on the
 industrial that means booting with half its RAM."
    info "  on-board EEPROM  OK"
}

profile_bb64_finish() { :; }


#############################################################################
# Shared phases
#############################################################################

write_fstab() {
    cat > "${DSTROOT}/etc/fstab"
    {
        echo "#####################################"
        echo "#/dev/sda1  ${MEDIADIR}  auto  defaults,nonempty,noatime,nodiratime,exec,nofail,flush,uid=1000,gid=1000  0  0"
        echo "#####################################"
    } >> "${DSTROOT}/etc/fstab"
}

preflight() {
    banner "Checking before flashing to ${DEVICE}"

    [ -n "${DEVICE}" ] || die "no target device given.  See ${PGM} --help."
    [ -b "/dev/${DEVICE}" ] || die "/dev/${DEVICE} is not a block device."

    # Partition naming: mmcblk0 -> mmcblk0p1, sda -> sda1.
    case "${DEVICE}" in
        mmcblk*|nvme*) DSTPART="/dev/${DEVICE}p" ;;
        *)             DSTPART="/dev/${DEVICE}"  ;;
    esac

    local rootdev bootdisk
    rootdev=$(findmnt -no SOURCE / | grep "^/dev/" || true)
    [ -n "${rootdev}" ] || die "cannot determine the booted root device."
    bootdisk=$(lsblk -no PKNAME "${rootdev}" 2>/dev/null | grep . | head -1 || true)
    # Two ways to say it, because lsblk cannot resolve a /dev/root-style symlink and
    # flashing over the running system is unrecoverable.
    if [ "${bootdisk}" = "${DEVICE}" ] || [ "${rootdev#/dev/${DEVICE}}" != "${rootdev}" ]; then
        die "/dev/${DEVICE} is the booted disk.  Refusing to flash onto it."
    fi

    # THE check the old Pi path was missing.  On platforms whose boot files live on
    # their own filesystem, that filesystem must be mounted or the copy silently
    # produces an empty boot partition.  An unmounted /boot/firmware is an empty
    # directory, and rsync of an empty directory succeeds.
    if [ "${SRC_BOOT_SEPARATE}" = "y" ]; then
        if ! mountpoint -q "${SRC_BOOT_DIR}"; then
            info "  ${SRC_BOOT_DIR} is not mounted; attempting to mount it..."
            mount "${SRC_BOOT_DIR}" 2>/dev/null || true
        fi
        # Resolved from fstab, not from the mount table: the mount table is empty
        # for exactly the case this diagnostic is for.
        local fstabsrc
        fstabsrc=$(findmnt --fstab -no SOURCE "${SRC_BOOT_DIR}" 2>/dev/null || true)
        # via the udev symlinks, not `blkid -t`: blkid rescans every block device,
        # which on a box with a large external disk attached can take long enough to
        # look like a hang -- while printing an error message.
        case "${fstabsrc}" in
            LABEL=*)    fstabsrc=$(readlink -f "/dev/disk/by-label/${fstabsrc#LABEL=}" 2>/dev/null || true) ;;
            UUID=*)     fstabsrc=$(readlink -f "/dev/disk/by-uuid/${fstabsrc#UUID=}" 2>/dev/null || true) ;;
            PARTUUID=*) fstabsrc=$(readlink -f "/dev/disk/by-partuuid/${fstabsrc#PARTUUID=}" 2>/dev/null || true) ;;
        esac
        [ -n "${fstabsrc}" ] || fstabsrc="<boot device>"
        mountpoint -q "${SRC_BOOT_DIR}" || die \
"${SRC_BOOT_DIR} is not mounted, so there are no boot files to copy.

 This is usually a failed boot-time fsck of the FAT partition; the system boots
 fine without it, so it can go unnoticed.  Check:

     journalctl -u $(systemd-escape -p --suffix=mount ${SRC_BOOT_DIR} 2>/dev/null || echo "${SRC_BOOT_DIR}")
     fsck.vfat -n ${fstabsrc}

 Flashing now would produce a destination that cannot boot."

        # --first-only: a directory can legitimately carry a stacked mount (someone
        # ran `mount /boot/firmware` on an already-mounted path), and findmnt then
        # prints one line per mount.  Taking all of them yields a multi-line value
        # that silently poisons every later use of it.
        SRC_BOOT_DEV=$(findmnt -no SOURCE --first-only "${SRC_BOOT_DIR}" 2>/dev/null || \
                       findmnt -no SOURCE "${SRC_BOOT_DIR}" | head -1)
        [ -b "${SRC_BOOT_DEV}" ] || die \
            "${SRC_BOOT_DIR} did not resolve to a block device (got '${SRC_BOOT_DEV}')."
        # And it must have real content -- a mounted but blank FAT is just as fatal.
        local n
        n=$(find "${SRC_BOOT_DIR}" -maxdepth 1 | wc -l)
        [ "${n}" -gt 3 ] || die "${SRC_BOOT_DIR} is mounted but nearly empty (${n} entries)."
        info "  source boot filesystem: ${SRC_BOOT_DEV} on ${SRC_BOOT_DIR} (${n} entries)  OK"
    else
        info "  boot files live on the root filesystem at ${SRC_BOOT_DIR}  OK"
    fi

    # Nothing on the target may be mounted -- including a media partition.  Deepest
    # path first, so a nested mount does not make its parent's umount fail EBUSY.
    # `|| true`: grep exits 1 when the target has nothing mounted, which under
    # pipefail+set -e would abort the flash before it started.
    local mounted
    mounted=$(lsblk -rno MOUNTPOINT "/dev/${DEVICE}" 2>/dev/null | grep . | sort -r || true)
    if [ -n "${mounted}" ]; then
        info "  unmounting existing filesystems on /dev/${DEVICE}:"
        local m
        for m in ${mounted}; do
            info "    ${m}"
            umount -R "${m}" || die "could not unmount ${m} from /dev/${DEVICE}."
        done
    fi

    # Space.  Only pay for the du over the media directory if the cheap check fails.
    local srcused dstsize
    srcused=$(df -B1 --output=used / | tail -1)
    dstsize=$(blockdev --getsize64 "/dev/${DEVICE}")
    if [ "${dstsize}" -le "${srcused}" ] && [ "${COPY_MEDIA}" != "y" ] \
       && ! mountpoint -q "${MEDIADIR}"; then
        info "  computing media size to see if the excluded content makes it fit..."
        srcused=$(( srcused - $(du -sxB1 "${MEDIADIR}" 2>/dev/null | cut -f1 || echo 0) ))
    fi
    info "  source uses $(( srcused / 1000000 ))MB, /dev/${DEVICE} is $(( dstsize / 1000000 ))MB"
    [ "${dstsize}" -gt "${srcused}" ] || die "/dev/${DEVICE} is too small for the source filesystem."

    info "  target /dev/${DEVICE} partitions will be ${DSTPART}1..  OK"

    "profile_${PROFILE}_preflight"

    PHASE_COMPLETED="${FUNCNAME[0]}"
}

# Wait for the kernel and udev to actually present the partition nodes.
#
# `udevadm settle` is not enough on its own: it returns success while the nodes are
# still missing (seen on a USB target, where sda1 existed and sda2 did not, and the
# following mkfs died with "/dev/sda2 does not exist").  Poll for the real thing.
wait_for_parts() {
    local want="$1" i p missing
    for i in $(seq 1 30); do
        missing=""
        for ((p = 1; p <= want; p++)); do
            [ -b "${DSTPART}${p}" ] || missing="${missing} ${DSTPART}${p}"
        done
        if [ -z "${missing}" ]; then
            [ "${i}" -eq 1 ] || info "  partition devices appeared after ${i}s"
            return 0
        fi
        udevadm settle >/dev/null 2>&1 || true
        sleep 1
    done
    die "partition devices${missing} never appeared after partitioning /dev/${DEVICE}."
}

do_partition() {
    banner "Partitioning /dev/${DEVICE}"
    wipefs -a "/dev/${DEVICE}" >/dev/null 2>&1 || true
    DST_NPARTS=""
    "profile_${PROFILE}_partition"
    [ -n "${DST_NPARTS}" ] || die "internal: profile ${PROFILE} did not set DST_NPARTS."
    sync
    blockdev --rereadpt "/dev/${DEVICE}" >/dev/null 2>&1 || true
    partprobe "/dev/${DEVICE}" >/dev/null 2>&1 || true
    wait_for_parts "${DST_NPARTS}"
    sfdisk -l "/dev/${DEVICE}"

    PHASE_COMPLETED="${FUNCNAME[0]}"
}

do_format() {
    banner "Formatting /dev/${DEVICE}"
    "profile_${PROFILE}_format"
    sync
    udevadm settle 2>/dev/null || sleep 2

    PHASE_COMPLETED="${FUNCNAME[0]}"
}

do_mount() {
    banner "Mounting the destination"
    mkdir -p "${DSTROOT}"
    "profile_${PROFILE}_mount"
    findmnt -R "${DSTROOT}"

    PHASE_COMPLETED="${FUNCNAME[0]}"
}

do_copy() {
    banner "Copying the root filesystem"

    local excludes=(
        '--exclude=/dev/*'
        '--exclude=/proc/*'
        '--exclude=/sys/*'
        '--exclude=/run/*'
        '--exclude=/tmp/*'
        '--exclude=/mnt/*'
        '--exclude=/media/*'
        '--exclude=/lost+found'
        '--exclude=/var/log/*'
        '--exclude=/var/swap'
    )
    [ "${COPY_MEDIA}" = "y" ]    || excludes+=( "--exclude=${MEDIADIR}/*" )
    [ "${KEEP_IDENTITY}" = "y" ] || excludes+=( '--exclude=/etc/ssh/*key*' )

    # Skip the boot directory entirely in this pass.  -x already stops rsync
    # *descending* into it, but rsync still recreates the mount-point directory and
    # applies its ownership and mode -- and by now the destination has a vfat mounted
    # right there, where chown/chmod are not supported.  That fails the transfer.  It
    # is copied properly below, onto the filesystem that belongs there.
    if [ "${SRC_BOOT_SEPARATE}" = "y" ]; then
        excludes+=( "--exclude=${SRC_BOOT_DIR}" )
    fi

    rsync -aAXxH --delete "${excludes[@]}" / "${DSTROOT}/"

    if [ "${SRC_BOOT_SEPARATE}" = "y" ]; then
        banner "Copying the boot filesystem (${SRC_BOOT_DIR})"
        # No -A/-X and a 2-second window: the destination is FAT, which has no ACLs
        # or xattrs and stores timestamps at 2-second granularity.
        rsync -rltD --delete --modify-window=2 \
            "${SRC_BOOT_DIR}/" "${DSTROOT}${DST_BOOT_DIR}/"
    fi

    PHASE_COMPLETED="${FUNCNAME[0]}"
}

scrub_identity() {
    banner "Cleaning up the copy"

    # Always -- these are noise on any copy, and stale logs on a new install are
    # actively confusing when diagnosing it later.
    find "${DSTROOT}/var/log/" -type f -delete 2>/dev/null || true
    rm -f "${DSTROOT}/root/.bash_history" "${DSTROOT}/root/.wget-hsts"
    rm -f "${DSTROOT}/home/${FPPUSER}/.bash_history"
    rm -rf "${DSTROOT}${MEDIADIR}"/logs/*

    # On btrfs, mark the directories that hold already-compressed content so nothing
    # written there later gets run through zstd for nothing.  mp3, fseq and video are
    # the bulk of what a player writes after the flash and none of it compresses --
    # measured 190MiB landing as 190.74MiB with the flag versus 6.08MiB without, on a
    # payload that actually was compressible.
    #
    # The flag is inherited by new files in the directory, which is the point.  It has
    # no effect on this pass, because the flash-time mount uses compress-force, which
    # overrides NOCOMPRESS by design; the installed fstab uses plain `compress`, which
    # honours it.
    if [ "${ROOTFS_TYPE}" = "btrfs" ] && command -v chattr >/dev/null 2>&1; then
        local d
        for d in music videos sequences images uploads cache; do
            [ -d "${DSTROOT}${MEDIADIR}/${d}" ] || mkdir -p "${DSTROOT}${MEDIADIR}/${d}" || continue
            chattr +m "${DSTROOT}${MEDIADIR}/${d}" 2>/dev/null \
                && info "  ${MEDIADIR}/${d}: compression disabled for future writes" \
                || info "  ${MEDIADIR}/${d}: could not set NOCOMPRESS (harmless)"
        done
    fi

    if [ "${KEEP_IDENTITY}" != "y" ]; then
        info "  resetting host identity (ssh host keys, machine-id)"
        rm -f "${DSTROOT}"/etc/ssh/*key*
        echo "uninitialized" > "${DSTROOT}/etc/machine-id"
        rm -f "${DSTROOT}/var/lib/dbus/machine-id"
        rm -rf "${DSTROOT}"/var/lib/connman/eth*
    else
        info "  keeping host identity (ssh host keys, machine-id)"
    fi

    # /var/log subdirectories that services expect to exist
    mkdir -p "${DSTROOT}/var/log/chrony" "${DSTROOT}/var/log/exim4"
    chown _chrony:_chrony "${DSTROOT}/var/log/chrony" 2>/dev/null || true
    chown Debian-exim:Debian-exim "${DSTROOT}/var/log/exim4" 2>/dev/null || true

    PHASE_COMPLETED="${FUNCNAME[0]}"
}

do_fixup() {
    banner "Configuring the copy to boot from /dev/${DEVICE}"
    "profile_${PROFILE}_fixup"
    info "  /etc/fstab:"
    sed 's/^/    /' "${DSTROOT}/etc/fstab"

    PHASE_COMPLETED="${FUNCNAME[0]}"
}

# The gate the old path never had.  Anything that reaches here has been copied and
# configured; this decides whether we are allowed to say so.
do_verify() {
    banner "Verifying the destination"
    local failed=0 f

    for f in ${VERIFY_ROOT_FILES}; do
        if [ -e "${DSTROOT}/${f}" ]; then
            info "  root: ${f}  OK"
        else
            echo "  root: ${f}  MISSING" >&2
            failed=1
        fi
    done

    for f in ${VERIFY_BOOT_FILES}; do
        if [ -e "${DSTROOT}${DST_BOOT_DIR}/${f}" ]; then
            info "  boot: ${f}  OK"
        else
            echo "  boot: ${f}  MISSING" >&2
            failed=1
        fi
    done

    if [ -n "${VERIFY_BOOT_GLOBS}" ]; then
        local found="" g globs
        IFS='|' read -ra globs <<< "${VERIFY_BOOT_GLOBS}"
        for g in "${globs[@]}"; do
            # shellcheck disable=SC2086
            if compgen -G "${DSTROOT}${DST_BOOT_DIR}/${g}" > /dev/null; then
                found="${g}"
                break
            fi
        done
        if [ -n "${found}" ]; then
            info "  boot: kernel (${found})  OK"
        else
            echo "  boot: no kernel matching ${VERIFY_BOOT_GLOBS}  MISSING" >&2
            failed=1
        fi
    fi

    # The destination's own fstab must point at the destination, not at this disk.
    # Comments are stripped first: write_fstab appends a commented /dev/sda1 media
    # example, which would otherwise satisfy this check on its own for a USB target.
    if grep -vE '^[[:space:]]*#' "${DSTROOT}/etc/fstab" \
         | grep -qE "(LABEL=${BOOT_LABEL:-__none__}|LABEL=${ROOT_LABEL:-__none__}|/dev/${DEVICE})"; then
        info "  root: /etc/fstab references ${DEVICE}  OK"
    else
        echo "  root: /etc/fstab does not reference ${DEVICE}  FAILED" >&2
        failed=1
    fi

    [ "${failed}" = "0" ] || die \
"the copy on /dev/${DEVICE} is incomplete and would not boot (see MISSING above).
 Nothing has been left mounted; /dev/${DEVICE} holds a partial copy and should be
 flashed again once the cause above is resolved."

    info ""
    info "  destination verified."

    PHASE_COMPLETED="${FUNCNAME[0]}"
}

# User-LED status, for the unattended factory flash where the console is nobody's
# and the board is the only thing to look at.  Three states, deliberately unmistakable
# for each other:
#
#   working  - a cylon sweep across the LEDs (needs a process, so it also proves the
#              flash has not wedged)
#   failed   - every LED blinking together, kernel-driven so it outlives this script
#   done     - the original triggers restored, or the board powered off
#
# Globbed, never indexed: BeagleBone Black exposes usr0-usr3 and PocketBeagle2
# exposes usr1-usr4.  The old cylon_leds() in BBB-FlashMMC.sh tested for usr0 before
# doing anything, so it silently did nothing at all on a PocketBeagle2 -- the LEDs
# just kept flickering on cpu/mmc activity.  On a Pi nothing matches and these are
# all no-ops.
LED_LIST=""
LED_SAVED=""
PROGRESS_LED_PID=""

leds_begin() {
    LED_LIST=$(ls -d /sys/class/leds/*usr[0-9] 2>/dev/null | sort || true)
    [ -n "${LED_LIST}" ] || return 0

    local l t
    local -a leds sweep
    read -ra leds <<< "$(echo ${LED_LIST})"

    for l in "${leds[@]}"; do
        t=$(sed -n 's/.*\[\([a-zA-Z0-9_-]*\)\].*/\1/p' "${l}/trigger" 2>/dev/null || true)
        LED_SAVED="${LED_SAVED}${l}=${t:-none} "
        echo none > "${l}/trigger" 2>/dev/null || true
    done

    # Forward, then back across the middle only, so the turn does not stutter:
    # for a b c d the cycle is a b c d c b.
    sweep=( "${leds[@]}" )
    local i
    for (( i = ${#leds[@]} - 2; i >= 1; i-- )); do
        sweep+=( "${leds[i]}" )
    done

    # The loop watches for its own parent going away, so the sweep cannot outlive the
    # flash even if no cleanup path runs.  Relying on leds_end alone already failed
    # once: the success path clears the EXIT trap, the subshell was orphaned to init,
    # and the LEDs cycled forever afterwards, reading as "still flashing".
    local parent=$$
    (
        while kill -0 "${parent}" 2>/dev/null; do
            for l in "${sweep[@]}"; do
                echo 1 > "${l}/brightness" 2>/dev/null || true
                sleep 0.12
                echo 0 > "${l}/brightness" 2>/dev/null || true
            done
        done
    ) &
    PROGRESS_LED_PID=$!
    return 0
}

leds_end() {
    if [ -n "${PROGRESS_LED_PID}" ]; then
        kill "${PROGRESS_LED_PID}" 2>/dev/null || true
        wait "${PROGRESS_LED_PID}" 2>/dev/null || true
        PROGRESS_LED_PID=""
    fi
    local entry l t
    for entry in ${LED_SAVED}; do
        l="${entry%%=*}"
        t="${entry##*=}"
        echo 0 > "${l}/brightness" 2>/dev/null || true
        echo "${t}" > "${l}/trigger" 2>/dev/null || true
    done
    return 0
}

# Leave every user LED blinking in unison to mark a failed flash.
#
# This exists for the unattended factory flash: the board is headless, nobody is
# watching the console, and the only other outcome is "powered off = done".  A board
# that failed must not be able to pass for a finished one.
#
# The kernel "timer" trigger is what makes it work -- it keeps blinking after this
# script exits, with no process left running to be killed or reaped.  All LEDs in
# lockstep is a pattern normal operation never produces (usr0 is a heartbeat, the
# others follow mmc and cpu activity).
#
# Globbed rather than indexed: BeagleBone Black exposes usr0-usr3 and PocketBeagle2
# exposes usr1-usr4, so any hardcoded index is wrong on one of them.  On a Pi nothing
# matches and this is a no-op.
signal_failure_leds() {
    local led found=0
    for led in /sys/class/leds/*usr[0-9]; do
        [ -e "${led}/trigger" ] || continue
        echo timer > "${led}/trigger" 2>/dev/null || continue
        echo 120 > "${led}/delay_on" 2>/dev/null || true
        echo 120 > "${led}/delay_off" 2>/dev/null || true
        found=1
    done
    if [ "${found}" = "1" ]; then
        echo "  All user LEDs are now blinking together to mark this failure." >&2
        echo "  They return to normal on the next reboot." >&2
    fi
    return 0
}

# Every exit path reports.  `set -e` terminates the shell at the failing command, so
# a phase gate downstream never gets to speak; without this an rsync that ran out of
# space just stopped, leaving the last thing on screen looking like ordinary output.
#
# Failure is the default outcome, and only reaching a specific point changes it.  An
# exit this script did not anticipate is therefore reported as a failure rather than
# passing silently.
on_exit() {
    local rc=$?
    leds_end
    cleanup_mounts
    if [ "${FLASH_OUTCOME}" = "failed" ]; then
        echo "" >&2
        echo "**********************************************************************" >&2
        echo "  FLASH FAILED -- ${DEVICE:+/dev/}${DEVICE:-the target device} is NOT" >&2
        echo "  bootable and must not be used.  See the error above." >&2
        echo "  This system is still running from its own disk." >&2
        echo "**********************************************************************" >&2
        signal_failure_leds
        echo "" >&2
        [ "${rc}" != "0" ] || rc=1
    fi
    exit "${rc}"
}

cleanup_mounts() {
    if mountpoint -q "${DSTROOT}"; then
        sync
        umount -R "${DSTROOT}" 2>/dev/null || umount -l -R "${DSTROOT}" 2>/dev/null || true
    fi
    rmdir "${DSTROOT}" 2>/dev/null || true
    rmdir "$(dirname "${DSTROOT}")" 2>/dev/null || true
}


#############################################################################
# Main
#############################################################################

detect_profile
DEVICE="${DEVICE#/dev/}"
"profile_${PROFILE}_init"

info "=========================================="
info " FPP flash to storage"
info "   platform    : ${FPPPLATFORM} (profile ${PROFILE})"
info "   target      : /dev/${DEVICE}"
info "   media       : $([ "${COPY_MEDIA}" = y ] && echo "copied" || echo "not copied")"
info "   identity    : $([ "${KEEP_IDENTITY}" = y ] && echo "kept" || echo "reset")"
info "   root fs     : ${ROOTFS_TYPE}"
info "=========================================="

preflight
phase_ok preflight

if [ "${DRYRUN}" = "y" ]; then
    banner "Dry run -- stopping before any change is made to /dev/${DEVICE}"
    FLASH_OUTCOME="dry-run"
    exit 0
fi

if [ "${ASSUME_YES}" != "y" ]; then
    echo ""
    printf "This will ERASE /dev/%s.  Continue? (yes/no): " "${DEVICE}"
    read -r resp
    case "${resp}" in
        y|yes) ;;
        *) FLASH_OUTCOME="aborted"; die "aborted at user request." ;;
    esac
fi

# From here the board is being written to.  Light the working pattern; on_exit puts
# the LEDs back however this ends.
leds_begin

# Each phase, then its gate, as separate top-level commands.  See phase_ok above for
# why the gate cannot live inside a helper.
do_partition
phase_ok do_partition

do_format
phase_ok do_format

do_mount
phase_ok do_mount

do_copy
phase_ok do_copy

scrub_identity
phase_ok scrub_identity

do_fixup
phase_ok do_fixup

do_verify
phase_ok do_verify

"profile_${PROFILE}_finish"

FLASH_OUTCOME="ok"
# leds_end explicitly, because `trap - EXIT` below means on_exit never runs on the
# success path.  Without it the sweep subshell is orphaned to init and keeps the LEDs
# cycling forever, which reads as "still flashing" long after the flash is done.
leds_end
cleanup_mounts
trap - EXIT

banner "Flash to /dev/${DEVICE} complete."

if [ "${DOREBOOT}" = "y" ]; then
    info "Powering down."
    systemctl poweroff || halt
fi
