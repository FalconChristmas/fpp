#!/bin/bash
#############################################################################
# expand_fpp_fs.sh - Pre-expand an FPP SD card (or image) on a Linux host
# so it can be sent off for mass duplication.
#
# A freshly flashed FPP card spends its first boot growing the root partition
# (fdisk + reboot) and its second resizing the filesystem
# (fpp-expand-rootfs.service). Doing that on every one of N duplicated cards
# is slow, and the old workaround -- boot the master card twice -- leaves
# per-device identity (SSH host keys, machine-id, UUID, random seed) and logs
# behind that then get cloned onto every copy.
#
# This script does the expansion on the host instead and leaves the card in
# "freshly built image" state, minus the expand step:
#
#   1. Grows the last (rootfs) partition to fill the card, less a small
#      reserve (see --reserve) so slightly-smaller cards of the "same" size
#      can still take a full copy.
#   2. e2fsck + resize2fs, zeroing the new inode tables now so the device's
#      ext4lazyinit thread doesn't spend the first boots doing it.
#   3. Removes the fpp_expand_rootfs marker and any fpp-expand-rootfs.service
#      enablement so the first boot doesn't try again.
#   4. Scrubs per-device state so every duplicate gets its own on first boot:
#      SSH host keys, machine-id, FPP UUID, systemd random seed/credential
#      secret, logs, journal, shell history, settings + config (unless
#      --keep-config), generated WiFi/network files, and macOS cruft on the
#      boot partition.
#
# Works for every FPP SD layout: Pi (p1 FAT boot, p2 rootfs) and BBB/BB64
# (p1 FAT boot, p2 swap, p3 rootfs) -- rootfs is always the last partition.
#
# Usage: sudo SD/expand_fpp_fs.sh [options] /dev/sdX
#        sudo SD/expand_fpp_fs.sh [options] fpp-image.img
#############################################################################

set -euo pipefail

usage() {
    cat <<EOF
Usage: $(basename "$0") [options] <device | image-file>

Expands the root filesystem of an FPP SD card (or .img file) to fill the
media and scrubs per-device state so the card is ready for duplication.

Options:
  -r, --reserve SIZE   Leave SIZE unpartitioned at the end of the card so the
                       copy fits on slightly smaller cards of the same nominal
                       size. Accepts a percentage (2%) or a size with an
                       optional K/M/G suffix (512M, 0). Default for a device:
                       2%, capped at 900M (kept under 1G so FPP doesn't flag
                       "SD card has unused space"). Default for an image: 0.
      --keep-config    Keep /home/fpp/media/settings and media/config/.
      --lazy-itable    Don't zero inode tables here (faster now, but the
                       device's ext4lazyinit does it during the first boots).
  -y, --yes            Don't ask for confirmation.
  -h, --help           Show this help.

For an image file, grow the file first if you want it bigger, e.g.
  truncate -s 7G fpp.img && sudo $(basename "$0") fpp.img
EOF
}

die() { echo "ERROR: $*" >&2; exit 1; }
log() { echo "==> $*"; }

ORIG_ARGS=("$@")
RESERVE="auto"
KEEP_CONFIG=0
ZERO_ITABLE=1
ASSUME_YES=0
TARGET=""

while [ $# -gt 0 ]; do
    case "$1" in
        -r|--reserve) RESERVE="${2:?--reserve needs a value}"; shift 2 ;;
        --reserve=*)  RESERVE="${1#*=}"; shift ;;
        --keep-config) KEEP_CONFIG=1; shift ;;
        --lazy-itable) ZERO_ITABLE=0; shift ;;
        -y|--yes) ASSUME_YES=1; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; die "unknown option $1" ;;
        *) [ -z "$TARGET" ] || die "only one target may be given"; TARGET="$1"; shift ;;
    esac
done
[ -n "$TARGET" ] || { usage >&2; exit 1; }

if [ "$(id -u)" -ne 0 ]; then
    exec sudo -- "$0" "${ORIG_ARGS[@]}"
fi

for cmd in sfdisk blockdev blkid partx e2fsck resize2fs dumpe2fs losetup udevadm unshare findmnt lsblk; do
    command -v "$cmd" >/dev/null 2>&1 || die "missing required command: $cmd"
done

#############################################################################
# Cleanup: unmount whatever we mounted, detach a loop device we attached.
#############################################################################
WORK_DIR=""
MOUNTS=()
LOOPDEV=""
cleanup() {
    local i
    for (( i=${#MOUNTS[@]}-1; i>=0; i-- )); do
        umount "${MOUNTS[$i]}" 2>/dev/null || umount -l "${MOUNTS[$i]}" 2>/dev/null || true
    done
    [ -n "$LOOPDEV" ] && losetup -d "$LOOPDEV" 2>/dev/null || true
    [ -n "$WORK_DIR" ] && rm -rf "$WORK_DIR" || true
}
trap cleanup EXIT

WORK_DIR="$(mktemp -d -t fpp-expand.XXXXXX)"

#############################################################################
# 1. Resolve the target to a whole-disk block device.
#############################################################################
IS_IMAGE=0
if [ -b "$TARGET" ]; then
    DEV="$(readlink -f "$TARGET")"
elif [ -f "$TARGET" ]; then
    IS_IMAGE=1
    LOOPDEV="$(losetup --show -f -P "$TARGET")"
    DEV="$LOOPDEV"
    [ "$RESERVE" = "auto" ] && RESERVE=0
else
    die "$TARGET is not a block device or image file"
fi
DEVNAME="$(basename "$DEV")"
[ -d "/sys/class/block/$DEVNAME" ] || die "$DEV is not a known block device"
[ -e "/sys/class/block/$DEVNAME/partition" ] && \
    die "$DEV is a partition; pass the whole device (e.g. /dev/sdc, not /dev/sdc1)"

# Never touch the disk the host is running from.
HOST_ROOT_DISK="$(lsblk -no PKNAME "$(findmnt -no SOURCE /)" 2>/dev/null | head -1 || true)"
[ -n "$HOST_ROOT_DISK" ] && [ "$HOST_ROOT_DISK" = "$DEVNAME" ] && \
    die "$DEV holds this host's root filesystem"

udevadm settle 2>/dev/null || true

#############################################################################
# 2. Find the partitions: the FAT boot partition and the rootfs, which is
#    always the last partition on every FPP layout.
#############################################################################
ROOT_PART="" ; ROOT_NUM=0 ; ROOT_START=-1 ; ROOT_SIZE=0
BOOT_PART=""
for sp in /sys/class/block/"$DEVNAME"/"$DEVNAME"*; do
    [ -f "$sp/partition" ] || continue
    pname="$(basename "$sp")"
    start="$(cat "$sp/start")"
    if [ "$start" -gt "$ROOT_START" ]; then
        ROOT_PART="/dev/$pname"
        ROOT_NUM="$(cat "$sp/partition")"
        ROOT_START="$start"
        ROOT_SIZE="$(cat "$sp/size")"
    fi
    if [ -z "$BOOT_PART" ] && [ "$(blkid -p -o value -s TYPE "/dev/$pname" 2>/dev/null || true)" = "vfat" ]; then
        BOOT_PART="/dev/$pname"
    fi
done
[ -n "$ROOT_PART" ] || die "no partitions found on $DEV"
ROOT_FSTYPE="$(blkid -p -o value -s TYPE "$ROOT_PART" 2>/dev/null || true)"
[ "$ROOT_FSTYPE" = "ext4" ] || die "last partition $ROOT_PART is '${ROOT_FSTYPE:-unknown}', expected ext4 rootfs"

# Desktop environments automount inserted cards. Unmount anything of ours
# that's mounted, but refuse if it's in use as swap or mounted somewhere that
# doesn't look like an automount.
while read -r name mp; do
    [ -n "$mp" ] || continue
    case "$mp" in
        /media/*|/run/media/*|/mnt/*) ;;
        *) die "$name is mounted at $mp; unmount it first" ;;
    esac
    log "Unmounting $name from $mp"
    umount "$mp"
done < <(lsblk -nrpo NAME,MOUNTPOINT "$DEV")
grep -q "^$DEV" /proc/swaps && die "a partition on $DEV is in use as swap"

#############################################################################
# 3. Confirm it's FPP before changing anything.
#############################################################################
ROOT_MNT="$WORK_DIR/root"
BOOT_MNT="$WORK_DIR/boot"
mkdir -p "$ROOT_MNT" "$BOOT_MNT"
mount -o ro,noload "$ROOT_PART" "$ROOT_MNT"
if [ ! -d "$ROOT_MNT/opt/fpp" ] || [ ! -d "$ROOT_MNT/home/fpp" ]; then
    umount "$ROOT_MNT"
    die "$ROOT_PART doesn't look like an FPP root filesystem (no /opt/fpp)"
fi
FPP_PLATFORM="$(cat "$ROOT_MNT/etc/fpp/platform" 2>/dev/null || echo unknown)"
FPP_RFS="$(cat "$ROOT_MNT/etc/fpp/rfs_version" 2>/dev/null || echo unknown)"
umount "$ROOT_MNT"

#############################################################################
# 4. Work out the new partition size.
#############################################################################
to_bytes() {   # 512M / 1G / 4096K / 1048576 -> bytes
    local v="${1^^}"
    case "$v" in
        *K) echo $(( ${v%K} * 1024 )) ;;
        *M) echo $(( ${v%M} * 1024 * 1024 )) ;;
        *G) echo $(( ${v%G} * 1024 * 1024 * 1024 )) ;;
        *)  echo $(( v )) ;;
    esac
}

SS="$(blockdev --getss "$DEV")"
DISK_BYTES="$(blockdev --getsize64 "$DEV")"
TOTAL_SECT=$(( DISK_BYTES / SS ))
ALIGN=$(( 1048576 / SS ))
START_SECT=$(( ROOT_START * 512 / SS ))
CUR_SECT=$(( ROOT_SIZE * 512 / SS ))

case "$RESERVE" in
    auto)
        RESERVE_BYTES=$(( DISK_BYTES * 2 / 100 ))
        CAP=$(( 900 * 1024 * 1024 ))
        [ "$RESERVE_BYTES" -gt "$CAP" ] && RESERVE_BYTES=$CAP
        ;;
    *%)
        [[ "${RESERVE%\%}" =~ ^[0-9]+$ ]] || die "bad --reserve value: $RESERVE"
        RESERVE_BYTES=$(( DISK_BYTES * ${RESERVE%\%} / 100 ))
        ;;
    *)
        [[ "$RESERVE" =~ ^[0-9]+[KkMmGg]?$ ]] || die "bad --reserve value: $RESERVE"
        RESERVE_BYTES="$(to_bytes "$RESERVE")"
        ;;
esac

PTTYPE="$(blkid -p -o value -s PTTYPE "$DEV" 2>/dev/null || true)"
END_SECT=$(( TOTAL_SECT - RESERVE_BYTES / SS ))
if [ "$PTTYPE" = "gpt" ]; then
    # The backup GPT lives at the end of the disk; the partition must stop
    # short of it. (It gets moved to the real end of the card below.)
    GPT_TAIL=$(( 1 + 16384 / SS ))
    [ "$END_SECT" -gt $(( TOTAL_SECT - GPT_TAIL )) ] && END_SECT=$(( TOTAL_SECT - GPT_TAIL ))
fi
END_SECT=$(( END_SECT / ALIGN * ALIGN ))
NEW_SECT=$(( END_SECT - START_SECT ))

GROW=1
if [ "$NEW_SECT" -le $(( CUR_SECT + ALIGN )) ]; then
    GROW=0
fi

human() { numfmt --to=iec-i --suffix=B "$1" 2>/dev/null || echo "$1 bytes"; }

echo
echo "Target:        $DEV$([ "$IS_IMAGE" = 1 ] && echo " (image $TARGET)")"
echo "Model:         $(lsblk -dno MODEL,VENDOR "$DEV" 2>/dev/null | xargs || true)"
echo "Size:          $(human "$DISK_BYTES")"
echo "FPP platform:  $FPP_PLATFORM (image $FPP_RFS)"
echo "Boot part:     ${BOOT_PART:-<none found>}"
echo "Root part:     $ROOT_PART ($(human $(( CUR_SECT * SS ))))"
if [ "$GROW" = 1 ]; then
    echo "Grow root to:  $(human $(( NEW_SECT * SS ))) (leaving $(human $(( (TOTAL_SECT - END_SECT) * SS ))) unpartitioned)"
else
    echo "Grow root to:  (already at or beyond target -- not changing partitions)"
fi
echo "Settings/config: $([ "$KEEP_CONFIG" = 1 ] && echo kept || echo "cleared")"
echo
lsblk -o NAME,SIZE,TYPE,FSTYPE,LABEL "$DEV"
echo
if [ "$ASSUME_YES" != 1 ]; then
    read -r -p "Modify $DEV as described above? Type 'yes' to continue: " ans
    [ "$ans" = "yes" ] || die "aborted"
fi

#############################################################################
# 5. Grow the partition.
#############################################################################
run_e2fsck() {
    local rc=0
    e2fsck -f -y "$1" || rc=$?
    # 0 = clean, 1 = errors fixed. Anything else means don't continue.
    [ "$rc" -le 1 ] || die "e2fsck on $1 failed (exit $rc)"
}

if [ "$GROW" = 1 ]; then
    if [ "$PTTYPE" = "gpt" ]; then
        log "Moving backup GPT to the end of the card"
        sfdisk --relocate gpt-bak-std "$DEV"
    fi
    log "Growing partition $ROOT_NUM to $(human $(( NEW_SECT * SS )))"
    echo "$START_SECT,$NEW_SECT" | sfdisk --no-reread --wipe never --wipe-partitions never -q -N "$ROOT_NUM" "$DEV"
    partx -u "$DEV" 2>/dev/null || blockdev --rereadpt "$DEV"
    udevadm settle 2>/dev/null || true
    KSIZE=$(( $(cat "/sys/class/block/$DEVNAME/$(basename "$ROOT_PART")/size") * 512 / SS ))
    [ "$KSIZE" -eq "$NEW_SECT" ] || die "kernel still sees old size for $ROOT_PART ($KSIZE != $NEW_SECT sectors); re-insert the card and rerun"
fi

#############################################################################
# 6. Check + resize the filesystem.
#############################################################################
log "Checking $ROOT_PART"
run_e2fsck "$ROOT_PART"

log "Resizing filesystem on $ROOT_PART"
if [ "$ZERO_ITABLE" = 1 ]; then
    # resize2fs skips zeroing new inode tables whenever the running kernel
    # advertises lazy_itable_init, leaving the work to the *device's*
    # ext4lazyinit thread, which then grinds through it during the first boots.
    # Hide that feature file in a private mount namespace so resize2fs does the
    # zeroing here instead.
    unshare --mount --propagation private /bin/sh -c '
        if [ -d /sys/fs/ext4/features ]; then
            mount -t tmpfs -o size=4k,mode=0555 fpp-expand /sys/fs/ext4/features || exit 1
        fi
        exec resize2fs -p "$1"' sh "$ROOT_PART"
else
    resize2fs -p "$ROOT_PART"
fi

#############################################################################
# 7. Clear the first-boot expand and scrub per-device state.
#############################################################################
log "Mounting filesystems to scrub per-device state"
mount "$ROOT_PART" "$ROOT_MNT"
MOUNTS+=("$ROOT_MNT")
if [ -n "$BOOT_PART" ]; then
    mount "$BOOT_PART" "$BOOT_MNT"
    MOUNTS+=("$BOOT_MNT")
fi
R="$ROOT_MNT"
MEDIA="$R/home/fpp/media"

# Expand marker: on the FAT partition for Pi/BB64 (/boot/firmware), on the
# rootfs (/boot) for BBB. Plus the resize2fs service a first boot enables.
rm -f "$BOOT_MNT/fpp_expand_rootfs" "$R/boot/fpp_expand_rootfs" "$R/boot/firmware/fpp_expand_rootfs"
rm -f "$R"/etc/systemd/system/*.wants/fpp-expand-rootfs.service
# Pre-FPP growers that the image builders already disable; belt and braces.
rm -f "$R/etc/bbb.io/growpart" "$R/resizerootfs"

# Per-device identity. Each is regenerated on first boot.
rm -f "$R"/etc/ssh/ssh_host_*
echo "uninitialized" > "$R/etc/machine-id"
rm -f "$R/var/lib/dbus/machine-id"
ln -sf /etc/machine-id "$R/var/lib/dbus/machine-id"
rm -f "$R/etc/fpp/fpp_uuid" "$MEDIA/config/fpp_uuid"
rm -f "$R/var/lib/systemd/random-seed" "$R/var/lib/systemd/credential.secret"
rm -f "$R"/var/lib/dhcp/*.leases

# Logs and history.
rm -rf "$R"/var/log/journal/*/
find "$R/var/log" -type f \( -name '*.gz' -o -name '*.xz' -o -name '*.[0-9]' -o -name '*.old' \) -delete
find "$R/var/log" -type f -exec truncate -s 0 {} +
rm -rf "$MEDIA"/logs/* "$MEDIA"/tmp/* "$MEDIA"/crashes/*
rm -f "$R/root/.bash_history" "$R/home/fpp/.bash_history" \
      "$R/root/.lesshst" "$R/home/fpp/.lesshst" \
      "$R/root/.ssh/known_hosts" "$R/home/fpp/.ssh/known_hosts"

if [ "$KEEP_CONFIG" = 1 ]; then
    if [ -f "$MEDIA/settings" ]; then
        sed -i -E 's/^(rebootFlag|restartFlag) *=.*/\1 = "0"/' "$MEDIA/settings"
    fi
else
    rm -f "$MEDIA/settings"
    rm -rf "$MEDIA"/config/backups/*
    find "$MEDIA/config" -maxdepth 1 -type f -delete 2>/dev/null || true
    # Files fppinit generates from config/interface.* -- they can hold WiFi
    # passwords, and their source config is gone.
    rm -f "$R"/etc/systemd/network/10-*.network "$R"/etc/wpa_supplicant/wpa_supplicant-wl*.conf
fi

if [ -n "$BOOT_PART" ]; then
    # A /boot/fpp/ default-config directory is copied into media on the first
    # boot of each device; don't let the master's "already copied" flag ship.
    rm -f "$BOOT_MNT/fpp/copy_done"
    # Cruft from plugging the card into a Mac.
    rm -rf "$BOOT_MNT/.Spotlight-V100" "$BOOT_MNT/.fseventsd" "$BOOT_MNT/.Trashes" \
           "$BOOT_MNT/.TemporaryItems" "$BOOT_MNT/System Volume Information"
    find "$BOOT_MNT" \( -name '._*' -o -name '.DS_Store' \) -delete 2>/dev/null || true
fi

if [ -s "$R/home/fpp/.ssh/authorized_keys" ] || [ -s "$R/root/.ssh/authorized_keys" ]; then
    echo "WARNING: authorized_keys present in /home/fpp/.ssh or /root/.ssh -- left in place;" >&2
    echo "         every duplicate will accept those SSH keys." >&2
fi

sync
for (( i=${#MOUNTS[@]}-1; i>=0; i-- )); do
    umount "${MOUNTS[$i]}"
done
MOUNTS=()

log "Final filesystem check"
run_e2fsck "$ROOT_PART"
sync

echo
lsblk -o NAME,SIZE,TYPE,FSTYPE,LABEL "$DEV"
echo
log "Done. $TARGET is expanded and ready for duplication."
