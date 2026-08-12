#!/bin/bash

# logStage "<message>": emit a "===== <message> =====" section header. Mirrors
# the helper in scripts/common (which upgradeOS-part2.sh sources) and drives the
# streaming FPP OS Upgrade dialog's status line via ParseLastStageMarker() in
# www/js/fpp.js. Defined locally rather than sourcing scripts/common because this
# script is copied to and run from /home/fpp/media/tmp, where common's $0-based
# path detection would misresolve FPPDIR.
logStage() {
    echo "===== $1 ====="
}

# Unified upgrade logging -> logs/fpp_system_upgrades.log, a local equivalent of
# startUpgradeLog/startOpLog in scripts/common (not sourced here, per above).
#
# This tee is the ONLY on-disk record of the copy itself, and it has to live in
# THIS script specifically:
#   - upgradeOS-part2.sh does the actual work but runs under `chroot /mnt`, where
#     the media tree is not mounted (part1 bind-mounts only /, boot, dev, proc
#     into /mnt) -- it cannot reach logs/ to write there itself. Its output flows
#     up this script's stdout, so tee'ing here captures part2 as well.
#   - part1 runs before the reboot on the still-intact system, and /home is never
#     touched by part2's rsync (bin etc lib opt root sbin usr var only), so the
#     log survives the image swap and lands in the next Support Zip.
#
# Best-effort: if logs/ is not writable the upgrade must still proceed, so fall
# back to plain stdout rather than failing a flash over a log file.
LOGFILE=/home/fpp/media/logs/fpp_system_upgrades.log
FPPDLOG=/home/fpp/media/logs/fppd.log
OPTARGET=$(/usr/bin/basename $1)
LOGGING=false
if [ -d /home/fpp/media/logs ] && [ -w /home/fpp/media/logs ]; then
    LOGGING=true
    exec > >(tee >(while IFS= read -r __line || [ -n "${__line}" ]; do printf '%(%Y-%m-%d %H:%M:%S)T [os-upgrade %s] %s\n' -1 "${OPTARGET}" "${__line}" >> "${LOGFILE}"; done))
    exec 2>&1
fi

# fppdLogLine <facility> <message...>: append one breadcrumb to fppd.log in
# fppd's own line shape. A local mirror of the helper in scripts/common, for the
# same reason logStage is duplicated above -- this script cannot source common.
# fppd.log is the timeline; an OS upgrade is the most disruptive thing that can
# happen to a box, so "the OS was reflashed at 21:44, rc=0" belongs in the spine
# even though the blow-by-blow lives in fpp_system_upgrades.log. Like the log above this
# survives the image swap: part2's rsync never touches /home.
# Best-effort by the same rule as LOGGING -- never fail a flash over a log line.
fppdLogLine() {
    local __fac="$1"
    shift
    local __now=${EPOCHREALTIME}
    local __usec=${__now#*.}
    printf '%(%Y-%m-%d %H:%M:%S)T.%s %s(%s) [%s] %s\n' "${__now%.*}" "${__usec:0:3}" \
        "$(/usr/bin/basename $0)" "$$" "${__fac}" "$*" >> "${FPPDLOG}" 2>/dev/null
}

# The FINISH marker is appended straight to the log rather than echoed through
# the tee: this script deliberately closes stdout/stderr before it exits (see the
# end of the file), so by the time an EXIT trap runs there is no stdout left to
# echo to and the marker -- including the rc of a failed flash, the single most
# useful line in the file -- would be silently dropped. Writing direct also means
# the early `exit 1` verification failures above are recorded identically.
if [ "${LOGGING}" = "true" ]; then
    OPDETAIL="detail: logs/fpp_system_upgrades.log"
else
    OPDETAIL="detail: console (logs/ not writable, not logged)"
fi
fppdLogLine "Upgrade" "os-upgrade START: ${OPTARGET} (${OPDETAIL})"
echo "===== os-upgrade START: ${OPTARGET} ====="
trap '__rc=$?; if [ "${LOGGING}" = "true" ]; then printf "%(%Y-%m-%d %H:%M:%S)T [os-upgrade %s] ===== os-upgrade FINISH: %s (rc=%s) =====\n" -1 "${OPTARGET}" "${OPTARGET}" "${__rc}" >> "${LOGFILE}" 2>/dev/null; fi; fppdLogLine "Upgrade" "os-upgrade FINISH: ${OPTARGET} (rc=${__rc}) (${OPDETAIL})"' EXIT

logStage "Verifying image"

FPPOS=`/usr/bin/basename $1`
GITHUBSIZE=`curl -fsSL http://127.0.0.1/api/git/releases/sizes | grep ${FPPOS} | awk -F, '{print $2}'`
OURSIZE=`/usr/bin/stat -c %s $1`

# Locate the boot partition, if there is a separate one.  `mount -o bind /` below
# is NOT recursive, so anything mounted under / is invisible through that bind and
# has to be bound in on its own.  What matters here is therefore "where is the boot
# partition mounted", not "where do the boot files live":
#   Raspberry Pi / BeagleBone 64 -> a FAT partition mounted on /boot/firmware
#   older images                 -> a boot partition mounted on /boot
#   BeagleBone Black             -> no separate boot partition at all; /boot is a
#                                   directory on the root filesystem and is already
#                                   covered by the bind of /
# The BBB is why this probes mounts rather than testing for a directory: current
# BBB images ship a /boot/firmware directory (ID.txt, START.HTM, sysconf.txt) that
# is not a mount and is not where the boot files live, so `-d /boot/firmware` bound
# the wrong path and left the real boot files unhandled.
BOOTMOUNT=""
for __d in /boot/firmware /boot; do
    if findmnt -n "${__d}" > /dev/null 2>&1; then
        BOOTMOUNT="${__d}"
        break
    fi
done
unset __d

if ! [[ $GITHUBSIZE =~ ^-?[0-9]+$ ]];
then
  echo "Couldn't get fppos size from Github, attempting upgrade anyway"
else
  if [ "$OURSIZE" -lt "$GITHUBSIZE" ];
  then
    echo "Download size seems too small. Our size: $OURSIZE, Github size: $GITHUBSIZE deleting $1"
    echo "Please try to download the fppos again"
    rm $1
    exit 1;
  else
    echo "fppos size matches Github, continuing"
  fi
fi

# The rsync in upgradeOS-part2.sh copies a new root filesystem over the LIVE
# one (bin etc lib opt root sbin usr var), so the running OS occupies the disk
# throughout the copy. If the root filesystem is too full for the incoming OS,
# the copy dies partway through and leaves a half-written root that will not
# boot (issue #2816). Pre-flight check: the image itself is already on disk
# (typically on this same filesystem), and the new OS is roughly the size of
# the old one, so requiring at least 2x the image size free is a safe margin
# for the in-place replacement plus the staging image.
FREESPACE=$(df -Pk / | awk 'NR==2 {print $4}')
if [[ $FREESPACE =~ ^[0-9]+$ ]]; then
  FREESPACE_BYTES=$((FREESPACE * 1024))
  REQUIRED=$((OURSIZE * 2))
  if [ "$FREESPACE_BYTES" -lt "$REQUIRED" ];
  then
    echo "Not enough free space on the root filesystem to apply this OS image."
    echo "Free: $((FREESPACE_BYTES / 1024 / 1024)) MB, required: $((REQUIRED / 1024 / 1024)) MB (2x image size)"
    echo "Aborting upgrade. Free up space and try again."
    exit 1;
  else
    echo "Sufficient free space on the root filesystem: $((FREESPACE_BYTES / 1024 / 1024)) MB free, $((REQUIRED / 1024 / 1024)) MB required"
  fi
else
  echo "Could not determine free space on root filesystem, continuing"
fi

mount $1 /mnt

ORIGTYPE=$(</etc/fpp/platform)
NEWTYPE=$(</mnt/etc/fpp/platform)

if [ "$ORIGTYPE" != "$NEWTYPE" ]; then
    echo "New image type '${NEWTYPE}' does not match existing '${ORIGTYPE}'"
    umount /mnt
    exit 1;
fi

# Architecture check: FPPPLATFORM doesn't distinguish 32-bit Pi from 64-bit
# Pi (both are "Raspberry Pi"), so applying a Pi64 .fppos on a Pi32 system
# (or vice versa) would brick the device. /etc/fpp/arch was added in FPP 10
# specifically for this guard. Only enforced when both sides have the marker.
if [ -f /etc/fpp/arch ] && [ -f /mnt/etc/fpp/arch ]; then
    ORIGARCH=$(</etc/fpp/arch)
    NEWARCH=$(</mnt/etc/fpp/arch)
    if [ "$ORIGARCH" != "$NEWARCH" ]; then
        echo "New image arch '${NEWARCH}' does not match existing '${ORIGARCH}'"
        umount /mnt
        exit 1;
    fi
fi

#make sure settings are re-applied after boot
echo "BootActions = \"settings\"" >> /home/fpp/media/settings


#remove files where the binary may not have changed (so rsync won't recopy)
#but the caps (getcap) might be different
rm -f /bin/ping

logStage "Preparing filesystems"
echo "Mounting filesystems for copy"
mount -o bind / /mnt/mnt
if [ -n "${BOOTMOUNT}" ]; then
    mount -o bind ${BOOTMOUNT} /mnt/mnt${BOOTMOUNT}
fi
mount -t tmpfs tmpfs /mnt/tmp
mount -o bind /dev /mnt/dev
mount -o bind /proc /mnt/proc

if [ -f /home/fpp/media/tmp/keepOptFPP ]
then
    # If we are on master and keeping /opt/fpp, run the existing part2 script
    echo "keepOptFPP flag exists, script will not copy /opt/fpp from image."
    echo "Passing control to existing upgradeOS-part2.sh from /opt/fpp"
    stdbuf --output=0 --error=0 chroot /mnt /mnt/opt/fpp/SD/upgradeOS-part2.sh
elif [ "${BOOTMOUNT}" = "/boot/firmware" -a ! -d "/mnt/boot/firmware" ]
then
    # Downgrading from Raspbian 12 or higher to a pre-12 version without /boot/firmware.
    # Keyed off BOOTMOUNT rather than `-d /boot/firmware` so the BBB -- which has that
    # directory but boots from /boot -- never lands here.
    echo "Downgrading to OS version without /boot/firmware."
    echo "Passing control to upgradeOS-part2.sh from current version."
    cp /opt/fpp/SD/upgradeOS-part2.sh /home/fpp/media/tmp/upgradeOS-part2.sh
    stdbuf --output=0 --error=0 chroot /mnt /mnt/home/fpp/media/tmp/upgradeOS-part2.sh
    rm /home/fpp/media/tmp/upgradeOS-part2.sh
else
    echo "Passing control to upgradeOS-part2.sh from fppos image"
    stdbuf --output=0 --error=0 chroot /mnt /opt/fpp/SD/upgradeOS-part2.sh
fi

echo "----------"
echo "Control returned from upgradeOS-part2.sh script, resuming upgradeOS-part1.sh"

logStage "Finalizing"
echo "Copy done, unmounting filesystems"
sync
umount /mnt/proc
umount /mnt/dev
umount /mnt/tmp
if [ -n "${BOOTMOUNT}" ]; then
    umount /mnt/mnt${BOOTMOUNT}
fi
umount /mnt/mnt

sync

echo "Please reboot if the system does not do so automatically"

exec 0>&- # close stdin
exec 1>&- # close stdout
exec 2>&- # close stderr
sleep 1
sync
