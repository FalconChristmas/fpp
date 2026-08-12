#!/bin/bash

. /opt/fpp/scripts/common
. /opt/fpp/scripts/functions

cd /

logStage "Updating boot filesystem"
echo "Running rsync to update boot file system:"
case "${FPPPLATFORM}" in
    "Raspberry Pi"|"BeagleBone 64")
        # Both Trixie-era Pi and BB64 use /boot/firmware as the FAT boot
        # mount. The old system being upgraded may still be on the older
        # /boot layout, so detect and remount if needed.
        ROOTDEV=$(findmnt -T /mnt | tail -n 1 | awk '{print $2}')
        # Use findmnt to avoid the ambiguous `mount | grep /boot` (which
        # could match unrelated lines inside the chroot).
        OLD_BOOT_AT_ROOT=false
        if findmnt -n /mnt/boot/firmware >/dev/null 2>&1; then
            BOOTMOUNTDEV=$(findmnt -n -o SOURCE /mnt/boot/firmware)
        elif findmnt -n /mnt/boot >/dev/null 2>&1; then
            BOOTMOUNTDEV=$(findmnt -n -o SOURCE /mnt/boot)
            OLD_BOOT_AT_ROOT=true
        else
            echo "ERROR: could not locate the old boot partition at /mnt/boot or /mnt/boot/firmware"
            exit 1
        fi

        if $OLD_BOOT_AT_ROOT; then
            echo "Remounting old boot partition from /mnt/boot to /mnt/boot/firmware"
            umount /mnt/boot
            mkdir -p /mnt/boot/firmware
            mount "${BOOTMOUNTDEV}" /mnt/boot/firmware
        fi

        rsync --outbuf=N -aAXxvc /boot/ /mnt/boot/ --delete-before

        if $OLD_BOOT_AT_ROOT; then
            echo "Adjusting /etc/fstab: /boot -> /boot/firmware"
            sed -e "s# /boot # /boot/firmware #" -i /mnt/etc/fstab
        fi

        # Pi uses cmdline.txt; BB64 uses extlinux.conf (updated later).
        if [ "${FPPPLATFORM}" = "Raspberry Pi" ]; then
            sed -i "s|root=/dev/[a-zA-Z0-9]* |root=${ROOTDEV} |g" /mnt/boot/firmware/cmdline.txt
        fi
        ;;
    *)
        # BeagleBone Black (still /boot, not /boot/firmware)
        rsync -aAXxvc /boot/ /mnt/boot/ --delete-during
        ;;
esac

echo

if [ "${FPPPLATFORM}" = "BeagleBone Black" ]; then
    # need to install the bootloader that goes with this version of the os
    echo "Updating Beagle boot loader:"
    /opt/fpp/bin.bbb/bootloader/install.sh
    echo
fi

# temporarily copy the ssh keys
echo "Saving system ssh keys"
mkdir tmp/ssh
cp -a mnt/etc/ssh/*key* tmp/ssh
echo

echo "Saving hostname"
mkdir tmp/etc
cp -a mnt/etc/hostname tmp/etc

echo "Saving machine-id"
cp -a mnt/etc/machine-id tmp/etc

# --- Reconcile fpp uid/gid with the incoming image --------------------------
# The fpp user's uid/gid has not been consistent across FPP's history (500 on
# older installs/images, 1000 since commit f8f2f1408 for Trixie compatibility).
# We only need the NEW uid/gid (this image's own /etc/passwd, unambiguous --
# no need to also track what it used to be); everything below just compares
# against what's actually on disk/in fstab right now and fixes it if it
# doesn't match, which is simpler and self-correcting regardless of history.
# If the rsync below (which overwrites the real /etc/passwd wholesale) changes
# the uid/gid, anything still owned by the old one becomes inaccessible to fpp
# after reboot; the "chown -R fpp:fpp mnt/home/fpp" below fixes /home/fpp on
# the root filesystem, but that's a non-recursive bind mount of / -- a
# separately mounted drive underneath it (e.g. media on an external USB/SSD,
# see /home/fpp/media in /etc/fstab) is invisible there and is handled below
# instead. Same class of bug as issue #2782.
NEW_FPP_UID=$(awk -F: -v u="${FPPUSER}" '$1==u{print $3}' /etc/passwd)
NEW_FPP_GID=$(awk -F: -v u="${FPPUSER}" '$1==u{print $4}' /etc/passwd)

if [ -n "${NEW_FPP_UID}" ] && [ -n "${NEW_FPP_GID}" ]; then
    # FAT/exFAT synthesizes ownership from the mount's uid=/gid= options rather
    # than storing it on disk, so a stale fstab entry (preserved verbatim by
    # the --exclude=etc/fstab below) would mount with a stale uid/gid after
    # reboot and lock ${FPPUSER} out of its own media. Match whatever uid=/gid=
    # is currently there to the incoming image, whatever it used to be.
    if grep -qE "/home/${FPPUSER}/media[[:space:]].*uid=[0-9]+,gid=[0-9]+" mnt/etc/fstab && \
       ! grep -qE "/home/${FPPUSER}/media[[:space:]].*uid=${NEW_FPP_UID},gid=${NEW_FPP_GID}" mnt/etc/fstab; then
        echo "FPP - Updating /home/${FPPUSER}/media mount options in fstab for the new uid/gid"
        sed -i -E "s/(\/home\/${FPPUSER}\/media[^\n]*uid=)[0-9]+(,gid=)[0-9]+/\1${NEW_FPP_UID}\2${NEW_FPP_GID}/" mnt/etc/fstab
    fi

    # ext4/btrfs persist real ownership on disk (no mount-time uid= option to
    # rewrite), so files written under a different uid need an explicit chown.
    # Only needed when media is a separate mount -- when it's on the root
    # filesystem, the chown below already covers it. Safe to do live/while
    # mounted, same as the chown copy_settings_to_storage.sh does after a
    # restore; chown on a currently-mounted FAT/exFAT filesystem is a no-op
    # since its ownership isn't stored per-file, so this is harmless there too.
    if mountpoint -q "mnt/home/${FPPUSER}/media"; then
        CURRENT_MEDIA_UID=$(stat -c %u "mnt/home/${FPPUSER}/media" 2>/dev/null)
        if [ -n "${CURRENT_MEDIA_UID}" ] && [ "${CURRENT_MEDIA_UID}" != "${NEW_FPP_UID}" ]; then
            echo "FPP - Updating ownership of /home/${FPPUSER}/media to the new uid/gid"
            chown -R "${NEW_FPP_UID}:${NEW_FPP_GID}" "mnt/home/${FPPUSER}/media" 2>/dev/null || true
        fi
    fi
fi

#remove some files that rsync won't copy over as they have the same timestamp and size, but are actually different
#possibly due to ACL's or xtended attributes
echo "Force cleaning files which do not sync properly"
# Detect multiarch triplet (arm-linux-gnueabihf, aarch64-linux-gnu, etc.)
# so this script works on both 32-bit and 64-bit Pi and on BB64.
TRIPLE=$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || gcc -dumpmachine 2>/dev/null)
rm -f mnt/bin/ping
rm -f mnt/lib/${TRIPLE}/librtmp.so.1
rm -f mnt/usr/bin/dc mnt/usr/bin/bc mnt/usr/bin/hardlink mnt/usr/bin/lua5*

SKIPFPP=""
if [ -f /mnt/home/fpp/media/tmp/keepOptFPP ]
then
    echo "Preserving existing /opt/fpp and ccache (cache dir + XDG config)"
    # ccache 4.12+ uses XDG paths (~/.cache/ccache, ~/.config/ccache);
    # older versions used ~/.ccache. Exclude all three so either layout
    # is preserved across the OS upgrade.
    SKIPFPP="--exclude=opt/fpp --exclude=root/.ccache --exclude=root/.cache/ccache --exclude=root/.config/ccache"
fi

#if kiosk was installed, save that state so after reboot, it can be re-installed
KIOSKMODE=$(getSetting Kiosk)
if [ "${KIOSKMODE}" = "1" ]; then
    touch /tmp/kiosk
fi

#copy everything other than fstab and the persistent net names
logStage "Updating root filesystem (longest step, please wait)"
echo "Running rsync to update / (root) file system:"
stdbuf --output=L --error=L rsync --outbuf=N -aAXxv bin etc lib opt root sbin usr var /mnt --delete-during --exclude=var/lib/php/sessions --exclude=etc/fstab --exclude=etc/systemd/network/10-*.network --exclude=etc/systemd/network/*-fpp-* --exclude=root/.ssh ${SKIPFPP}
echo

# force copy a few libs that rsync won't replace cleanly (same size/mtime
# but different content via xattrs/ACLs). Use globs so we don't hard-code
# package versions that drift with each OS release, and TRIPLE so this
# works on both armhf and aarch64.
force_copy_libs() {
    local pattern
    for pattern in "$@"; do
        local src
        for src in usr/lib/${TRIPLE}/${pattern}; do
            [ -e "$src" ] || continue
            cp -af "$src" "mnt/$src"
        done
    done
}
force_copy_libs 'libzip.so.*' 'libfribidi.so.*' 'libbrotlicommon.so.*'

echo "Adjusting fstab"
sed -i 's|tmpfs\s*/tmp\s*tmpfs.*||g' mnt/etc/fstab
sed -i 's|tmpfs\s*/var/tmp\s*tmpfs.*||g' mnt/etc/fstab

#restore the ssh keys
echo "Restoring system ssh keys"
cp -a tmp/ssh/* mnt/etc/ssh
echo

# Neutralize the incoming image's FIRST-BOOT identity setup.
#
# Both markers below exist so that a freshly flashed card gives itself a unique
# identity the first time it boots. An in-place OS upgrade is not a fresh flash:
# the device already has an identity, which the restores here deliberately
# preserve, so letting the image's markers through undoes that work on the very
# next boot -- after this script has already "restored" everything.
#
#   /etc/bbb.io/ssh_regenerate     bbbio-set-sysconf (generic-sys-mods) sees it
#                                  and runs bb-regenerate-ssh-host-keys, which
#                                  deletes /etc/ssh/ssh_host_* and runs
#                                  ssh-keygen -A. That is what made every OS
#                                  upgrade hand out new host keys and greet the
#                                  user with "REMOTE HOST IDENTIFICATION HAS
#                                  CHANGED" (the keys restored above were wiped
#                                  ~1 minute later, on first boot).
#   sysconf.txt user_name/password  written into the image by the build script
#                                  for first-boot user creation. bbbio-set-sysconf
#                                  applies them, resetting the user's password
#                                  back to the image default even if it had been
#                                  changed.
#
# Acting on either also triggers an extra "Rebooting after setting up
# sysconf.txt options" reboot immediately after the upgrade.
#
# Both are cleaned unconditionally: they only exist on Beagle images, and rm/sed
# on an absent file is a no-op everywhere else.
rm -f mnt/etc/bbb.io/ssh_regenerate
for SYSCONF in mnt/boot/firmware/sysconf.txt mnt/boot/sysconf.txt; do
    if [ -f "${SYSCONF}" ]; then
        sed -i -E '/^[[:space:]]*(user_name|user_password)[[:space:]]*=/d' "${SYSCONF}"
    fi
done

echo "Restoring hostname"
cp -af tmp/etc/hostname mnt/etc/hostname
rm -f  tmp/etc/hostname
echo 

echo "Restoring machine-id"
cp -af tmp/etc/machine-id mnt/etc/machine-id
rm -f  tmp/etc/machine-id
echo


#create a file in root to mark it as requiring kiosk mode to be installed, will be checked on reboot
if [ -f tmp/kiosk ]; then
    touch mnt/fpp_kiosk
fi

# make sure the stuff in fpp's home directory has correct owner/group
echo "Resetting ownership of files in /home/fpp"
chown -R fpp:fpp mnt/home/fpp

# create a file in root to detect that we just did an FPPOS Upgrade
touch mnt/fppos_upgraded

# Old images shipped a first-boot unit that regenerated the host keys; with the
# keys restored above it must not run. Gate on the key types OpenSSH actually
# has today: this test used to require ssh_host_dsa_key as well, which Debian
# has not shipped in years (OpenSSH dropped DSA entirely), so it never fired.
if [ -f mnt/etc/ssh/ssh_host_ed25519_key -a -f mnt/etc/ssh/ssh_host_ed25519_key.pub -a -f mnt/etc/ssh/ssh_host_rsa_key -a -f mnt/etc/ssh/ssh_host_rsa_key.pub ]
then
    if [ -f mnt/etc/systemd/system/multi-user.target.wants/regenerate_ssh_host_keys.service ]; then
        echo "Found all SSH key files, disabling first-boot SSH key regeneration"
        rm mnt/etc/systemd/system/multi-user.target.wants/regenerate_ssh_host_keys.service
    fi
else
    echo "WARNING: SSH host keys were not restored - clients will see a changed host key"
fi

if [ "${FPPPLATFORM}" = "Raspberry Pi" ]; then
    logStage "Updating boot loader"
    echo "Updating Raspberry Pi boot loader:"
    # This script runs chrooted into the read-only .fppos image mount, so the
    # default backup path (/var/lib/raspberrypi/bootloader/backup) and the
    # default boot partition (/boot/firmware, where it writes pieeprom.sig/
    # .upd) both resolve there and fail with "Read-only file system". Point
    # both at /mnt instead, which from inside this chroot is the bind-mounted
    # real disk that the new FPP will actually boot from.
    FIRMWARE_BACKUP_DIR=/mnt/var/lib/raspberrypi/bootloader/backup BOOTFS=/mnt/boot/firmware /bin/rpi-eeprom-update -a
fi

if [ "${FPPPLATFORM}" = "BeagleBone 64" ]; then
    if [ -f "/dev/mmcblk0p1" ]; then
        # need to install the bootloader to the eMMC that goes with this version of the os
        logStage "Updating boot loader"
        echo "Updating Beagle boot loader:"
        /opt/u-boot/bb-u-boot-pocketbeagle2/install-emmc.sh
        echo
    fi
    NEWROOT=$(findmnt -n -o SOURCE -f /mnt)
    DEVICE="${NEWROOT%p?}"
    sed -i "s|root=/dev/[a-zA-Z0-9]*\([0-9]\) |root=${DEVICE}p3 |g" /mnt/boot/firmware/extlinux/extlinux.conf
    sed -i "s|resume=/dev/[a-zA-Z0-9]*\([0-9]\) |resume=${DEVICE}p2 |g" /mnt/boot/firmware/extlinux/extlinux.conf
fi

echo "Running sync command to flush data"
sync

echo "Sync complete"
sleep 3

exit
