#!/bin/bash
#############################################################################
# build-image-pi.sh - Automated FPP image builder for Raspberry Pi
#
# Supports both 32-bit (armhf / "Pi") and 64-bit (arm64 / "Pi64") Raspberry
# Pi OS Lite base images. Produces a flashable .img.zip and a .fppos squashfs
# for in-place OS upgrades.
#
# Runs on any x86_64 or arm64 Linux host with qemu-user-static registered via
# binfmt_misc. The build works on the host by loop-mounting the Raspbian base
# image, growing the root partition, chrooting in, running FPP_Install.sh in
# non-interactive mode, and finalizing the image.
#
# Prereqs (Debian/Ubuntu):
#   apt-get install qemu-user-static binfmt-support parted kpartx \
#       zerofree squashfs-tools zip xz-utils wget ca-certificates e2fsprogs
#
# This script is intended to be invoked from CI (see .github/workflows/) but
# is also usable on a developer machine.
#############################################################################

set -euo pipefail

#############################################################################
# Defaults (overridable via args or env)
#############################################################################
ARCH="${ARCH:-armhf}"                        # armhf => Pi, arm64 => Pi64
VERSION="${VERSION:-}"                       # FPP version, e.g. 10.0
OSVERSION="${OSVERSION:-$(date +%Y-%m)}"     # Tag embedded in .fppos name
FPPBRANCH="${FPPBRANCH:-master}"
BASE_IMAGE_DATE="${BASE_IMAGE_DATE:-2025-12-04}"
BASE_IMAGE_URL="${BASE_IMAGE_URL:-}"
BASE_IMAGE_SHA256="${BASE_IMAGE_SHA256:-}"
IMG_SIZE_MB="${IMG_SIZE_MB:-7200}"           # Final raw image size
WORK_DIR="${WORK_DIR:-$(pwd)/build}"
OUTPUT_DIR="${OUTPUT_DIR:-$(pwd)/output}"
KEEP_WORK="${KEEP_WORK:-0}"                  # 1 => leave WORK_DIR populated
SKIP_FPPOS="${SKIP_FPPOS:-0}"                # 1 => skip squashfs generation
SKIP_ZIP="${SKIP_ZIP:-0}"                    # 1 => leave raw .img only, no zip
USE_LOCAL_SRC="${USE_LOCAL_SRC:-0}"          # 1 => seed /opt/fpp from local tree
FPP_SRC_DIR="${FPP_SRC_DIR:-$(readlink -f "$(dirname "$(readlink -f "$0")")/..")}"
SKIP_KERNEL_UPDATE="${SKIP_KERNEL_UPDATE:-0}"    # 1 => skip rpi-update next

usage() {
    cat <<EOF
Usage: $0 [options]

Options:
  --arch {armhf|arm64}     Target architecture (default: armhf)
                           armhf => FPP-vX-Pi.img.zip
                           arm64 => FPP-vX-Pi64.img.zip
  --version VER            FPP version string (required, e.g. 10.0)
  --os-version VER         OS version tag for .fppos (default: YYYY-MM)
  --branch BRANCH          FPP git branch to install (default: master)
  --base-image-url URL     Override base image URL
  --base-image-date DATE   Raspbian release date (default: 2025-12-04)
  --base-image-sha256 HEX  Optional sha256 of the .img.xz to verify
  --img-size-mb N          Raw output image size in MiB (default: 7200)
  --work-dir DIR           Scratch dir (default: ./build)
  --output-dir DIR         Artifact dir (default: ./output)
  --skip-kernel-update     Skip rpi-update of the kernel (faster iteration;
                           image will boot on whatever kernel the base image
                           shipped with — fine for installer-flow testing,
                           NOT a release-quality image)
  --skip-fppos             Do not produce .fppos squashfs
  --skip-zip               Do not zip the raw .img (faster iteration when
                           you're flashing the raw image directly to an SD
                           card for local testing)
  --use-local-src          Seed /opt/fpp in the image from the local FPP
                           working tree (FPP_SRC_DIR) instead of cloning from
                           github. Lets you iterate on source changes without
                           committing/pushing.
  --fpp-src-dir DIR        Path to local FPP checkout (default: parent of this
                           script). Used for the local FPP_Install.sh and,
                           when --use-local-src is set, for /opt/fpp.
  --keep-work              Keep working directory on success
  -h, --help               Show this help

Environment variables with the same names (uppercase, dashes => underscores)
are honored as defaults.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --arch)              ARCH="$2"; shift 2 ;;
        --version)           VERSION="$2"; shift 2 ;;
        --os-version)        OSVERSION="$2"; shift 2 ;;
        --branch)            FPPBRANCH="$2"; shift 2 ;;
        --base-image-url)    BASE_IMAGE_URL="$2"; shift 2 ;;
        --base-image-date)   BASE_IMAGE_DATE="$2"; shift 2 ;;
        --base-image-sha256) BASE_IMAGE_SHA256="$2"; shift 2 ;;
        --img-size-mb)       IMG_SIZE_MB="$2"; shift 2 ;;
        --work-dir)          WORK_DIR="$2"; shift 2 ;;
        --output-dir)        OUTPUT_DIR="$2"; shift 2 ;;
        --skip-kernel-update) SKIP_KERNEL_UPDATE=1; shift ;;
        --skip-fppos)        SKIP_FPPOS=1; shift ;;
        --skip-zip)          SKIP_ZIP=1; shift ;;
        --use-local-src)     USE_LOCAL_SRC=1; shift ;;
        --fpp-src-dir)       FPP_SRC_DIR="$(readlink -f "$2")"; shift 2 ;;
        --keep-work)         KEEP_WORK=1; shift ;;
        -h|--help)           usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

#############################################################################
# Validate + derive
#############################################################################
case "$ARCH" in
    armhf) PLATFORM_SUFFIX="Pi";   QEMU_BIN="qemu-arm-static"     ;;
    arm64) PLATFORM_SUFFIX="Pi64"; QEMU_BIN="qemu-aarch64-static" ;;
    *) echo "ARCH must be armhf or arm64 (got: $ARCH)" >&2; exit 2 ;;
esac

if [ -z "$VERSION" ]; then
    echo "--version is required (e.g. --version 10.0)" >&2
    exit 2
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root (needs losetup, mount, chroot)." >&2
    exit 1
fi

if [ -z "$BASE_IMAGE_URL" ]; then
    BASE_IMAGE_URL="https://downloads.raspberrypi.com/raspios_lite_${ARCH}/images/raspios_lite_${ARCH}-${BASE_IMAGE_DATE}/${BASE_IMAGE_DATE}-raspios-trixie-${ARCH}-lite.img.xz"
fi

LOCAL_INSTALLER="${FPP_SRC_DIR}/SD/FPP_Install.sh"
if [ ! -f "$LOCAL_INSTALLER" ]; then
    echo "Local FPP_Install.sh not found at $LOCAL_INSTALLER" >&2
    echo "Pass --fpp-src-dir to point at your FPP checkout." >&2
    exit 1
fi

if [ "$USE_LOCAL_SRC" = "1" ] && [ ! -d "$FPP_SRC_DIR/.git" ]; then
    echo "--use-local-src requires $FPP_SRC_DIR to be a git checkout" >&2
    exit 1
fi

# When seeding from the local tree, default FPPBRANCH to the local checkout's
# current branch so FPP_Install.sh's internal `git checkout ${FPPBRANCH}` is
# a no-op (instead of silently switching off your working branch). Only apply
# when FPPBRANCH wasn't explicitly overridden from the default "master".
if [ "$USE_LOCAL_SRC" = "1" ] && [ "$FPPBRANCH" = "master" ]; then
    LOCAL_BRANCH="$(git -C "$FPP_SRC_DIR" rev-parse --abbrev-ref HEAD 2>/dev/null || true)"
    if [ -n "$LOCAL_BRANCH" ] && [ "$LOCAL_BRANCH" != "HEAD" ]; then
        FPPBRANCH="$LOCAL_BRANCH"
    fi
fi

REQUIRED_CMDS=(wget xz parted losetup e2fsck e2label resize2fs fatlabel
               zerofree zip sha256sum mksquashfs rsync "$QEMU_BIN")
for cmd in "${REQUIRED_CMDS[@]}"; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Missing required command: $cmd" >&2
        exit 1
    fi
done

mkdir -p "$WORK_DIR" "$OUTPUT_DIR"
WORK_DIR="$(readlink -f "$WORK_DIR")"
OUTPUT_DIR="$(readlink -f "$OUTPUT_DIR")"

cat <<EOF
============================================================
FPP Image Build
  Architecture : $ARCH ($PLATFORM_SUFFIX)
  FPP version  : $VERSION
  OS version   : $OSVERSION
  Branch       : $FPPBRANCH
  Base image   : $BASE_IMAGE_URL
  Local installer: $LOCAL_INSTALLER
  Local src    : $( [ "$USE_LOCAL_SRC" = "1" ] && echo "$FPP_SRC_DIR (seeded into /opt/fpp)" || echo "(not used; installer clones from github)" )
  Kernel update: $( [ "$SKIP_KERNEL_UPDATE" = "1" ] && echo "SKIPPED (--skip-kernel-update)" || echo "rpi-update next (PRUNE_MODULES=1 WANT_PI5=1)" )
  Image size   : ${IMG_SIZE_MB} MiB
  Work dir     : $WORK_DIR
  Output dir   : $OUTPUT_DIR
============================================================
EOF

#############################################################################
# Cleanup trap
#############################################################################
LOOPDEV=""
declare -a MOUNTS=()
ROOT_MNT=""

cleanup() {
    set +e
    # unmount in reverse order
    local i
    for (( i=${#MOUNTS[@]}-1; i>=0; i-- )); do
        umount "${MOUNTS[$i]}" 2>/dev/null || umount -l "${MOUNTS[$i]}" 2>/dev/null
    done
    if [ -n "$LOOPDEV" ]; then
        losetup -d "$LOOPDEV" 2>/dev/null
    fi
    if [ "$KEEP_WORK" != "1" ] && [ -n "$ROOT_MNT" ] && [ -d "$ROOT_MNT" ]; then
        rmdir "$ROOT_MNT" 2>/dev/null
    fi
}
trap cleanup EXIT

#############################################################################
# 1. Download base image
#############################################################################
cd "$WORK_DIR"

BASE_XZ="$(basename "$BASE_IMAGE_URL")"
BASE_IMG="${BASE_XZ%.xz}"

if [ ! -f "$BASE_XZ" ]; then
    echo "[1/8] Downloading base image..."
    wget -O "${BASE_XZ}.part" "$BASE_IMAGE_URL"
    mv "${BASE_XZ}.part" "$BASE_XZ"
else
    echo "[1/8] Base image already cached: $BASE_XZ"
fi

if [ -n "$BASE_IMAGE_SHA256" ]; then
    echo "      Verifying sha256..."
    echo "${BASE_IMAGE_SHA256}  ${BASE_XZ}" | sha256sum -c -
else
    echo "      (no sha256 provided; skipping verification)"
fi

#############################################################################
# 2. Decompress + grow image file
#############################################################################
WORK_IMG="fpp-${PLATFORM_SUFFIX}-v${VERSION}.img"

echo "[2/8] Decompressing and preparing working image..."
rm -f "$WORK_IMG"
xz -dc "$BASE_XZ" > "$WORK_IMG"
truncate -s "${IMG_SIZE_MB}M" "$WORK_IMG"

#############################################################################
# 3. Grow root partition + attach loop device + resize FS
#############################################################################
echo "[3/8] Growing root partition to fill image..."
# Raspbian layout: p1=FAT32 boot, p2=ext4 rootfs. parted handles file targets
# fine; the "Warning: not all of the space available..." nag is fatal without
# ---pretend-input-tty, so use the scripted 'unit %' form instead.
parted -s "$WORK_IMG" resizepart 2 100%

# --direct-io=on bypasses the host's page cache for the loop device. Without
# it, NVMe-backed loop I/O is double-cached and bottlenecks on the loop's
# single I/O queue -- mksquashfs against the loop-mounted rootfs ends up
# many times slower than against a real block device.
LOOPDEV="$(losetup --show -f -P --direct-io=on "$WORK_IMG" 2>/dev/null)" \
    || LOOPDEV="$(losetup --show -f -P "$WORK_IMG")"
BOOTP="${LOOPDEV}p1"
ROOTP="${LOOPDEV}p2"

# Settle time for udev to create partition devices
for _ in 1 2 3 4 5; do
    [ -b "$BOOTP" ] && [ -b "$ROOTP" ] && break
    sleep 1
done
[ -b "$ROOTP" ] || { echo "Partition device $ROOTP not found" >&2; exit 1; }

echo "      fsck + resize2fs on $ROOTP"
e2fsck -fy "$ROOTP" || true   # e2fsck returns 1 for fixed errors, tolerate
resize2fs "$ROOTP"

# Ensure the FS labels match what FPP_Install.sh writes into /etc/fstab.
# Raspbian Lite historically labels p1 as "bootfs"; FPP expects "boot".
echo "      Setting filesystem labels (boot / rootfs)"
if command -v fatlabel >/dev/null 2>&1; then
    fatlabel "$BOOTP" boot 2>/dev/null || \
        echo "      (warning: fatlabel on $BOOTP failed)"
fi
e2label "$ROOTP" rootfs 2>/dev/null || \
    echo "      (warning: e2label on $ROOTP failed)"

#############################################################################
# 4. Mount + apply pre-boot tweaks
#############################################################################
echo "[4/8] Mounting and applying pre-boot configuration..."
ROOT_MNT="$(mktemp -d -t fpp-rootfs.XXXXXX)"
mount "$ROOTP" "$ROOT_MNT"
MOUNTS+=("$ROOT_MNT")
mkdir -p "$ROOT_MNT/boot/firmware"
mount "$BOOTP" "$ROOT_MNT/boot/firmware"
MOUNTS+=("$ROOT_MNT/boot/firmware")

CMDLINE="$ROOT_MNT/boot/firmware/cmdline.txt"
if [ -f "$CMDLINE" ]; then
    cp "$CMDLINE" "${CMDLINE}.fpp-orig"
    # Disable Raspbian's first-boot script entirely. FPP_Install.sh in --img
    # mode creates the fpp user itself, and the expand-rootfs behavior is
    # driven by the /boot/firmware/fpp_expand_rootfs marker touched below.
    sed -i \
        -e 's# init=/usr/lib/raspberrypi-sys-mods/firstboot##g' \
        -e 's# init=/usr/lib/raspi-config/init_resize\.sh##g' \
        -e 's# quiet_resize##g' \
        -e 's# resize2fs_once##g' \
        -e 's# resize##g' \
        "$CMDLINE"
fi
touch "$ROOT_MNT/boot/firmware/ssh"

# Writeable resolv.conf for apt/curl inside chroot. Some base images symlink
# /etc/resolv.conf into /run/systemd/resolve/, which we'd blow away when we
# tmpfs-mount /run below; remove the symlink and write a real file.
rm -f "$ROOT_MNT/etc/resolv.conf"
cp -L /etc/resolv.conf "$ROOT_MNT/etc/resolv.conf"

# qemu-user-static for chroot execution of foreign-arch binaries
install -m 0755 "$(command -v "$QEMU_BIN")" "$ROOT_MNT/usr/bin/$QEMU_BIN"

# Bind mounts for chroot. /run gets a fresh tmpfs rather than a bind from
# the host -- otherwise the host's /run/systemd/system leaks in (we've seen
# masked tmp.mount entries on the host break `systemctl enable tmp.mount`
# inside the chroot).
for d in dev dev/pts proc sys; do
    mkdir -p "$ROOT_MNT/$d"
    mount --bind "/$d" "$ROOT_MNT/$d"
    MOUNTS+=("$ROOT_MNT/$d")
done
mkdir -p "$ROOT_MNT/run"
mount -t tmpfs -o mode=755 tmpfs "$ROOT_MNT/run"
MOUNTS+=("$ROOT_MNT/run")

#############################################################################
# 5. Run FPP_Install.sh inside chroot
#############################################################################
echo "[5/8] Staging local FPP_Install.sh into chroot..."
install -m 0700 "$LOCAL_INSTALLER" "$ROOT_MNT/root/FPP_Install.sh"

INSTALLER_EXTRA_ARGS=""
if [ "$USE_LOCAL_SRC" = "1" ]; then
    echo "      Seeding /opt/fpp from $FPP_SRC_DIR ..."
    mkdir -p "$ROOT_MNT/opt/fpp"
    # Exclude build artifacts to keep the seeded tree lean; FPP_Install.sh
    # runs 'make clean; make optimized' so any host-built binaries would be
    # wasted bytes (and wrong-arch) in the image anyway.
    rsync -aH --delete \
        --exclude='/build/' \
        --exclude='/output/' \
        --exclude='**/*.o' \
        --exclude='**/*.so' \
        --exclude='**/*.dylib' \
        --exclude='src/fppd' \
        --exclude='src/fpp' \
        --exclude='src/fppmm' \
        --exclude='src/fppoled' \
        --exclude='src/fsequtils' \
        --exclude='src/fppcapedetect' \
        --exclude='src/fpprtc' \
        --exclude='src/fppinit' \
        --exclude='src/pch/' \
        "${FPP_SRC_DIR}/" "$ROOT_MNT/opt/fpp/"
    INSTALLER_EXTRA_ARGS="--skip-clone"
fi

# Pre-stage a fresh rpi-update from upstream so the in-chroot run doesn't
# need to TLS-fetch its own self-update (which hangs hard under qemu-arm).
# Also resolve the firmware branch -> commit SHA on the host: rpi-update's
# get_long_hash() makes a github API call that fails under qemu-arm even
# when bulk firmware downloads (different code path) succeed.
# Which rpi-firmware branch to pull kernel+firmware from.
#   master    = latest (currently 6.18.22, bumped 2026-04-13)
#   stable    = LTS-style (currently 6.12.75)
#   next      = historically the "latest" branch, marked DORMANT 2026-03-18
#   oldstable = 6.1, ancient
# We want master for FPP 10 (needs 6.18+). If master ever regresses or a
# specific-SHA pin is desired, override via env var.
RPI_FW_BRANCH="${RPI_FW_BRANCH:-master}"
RPI_FW_SHA=""
if [ "$SKIP_KERNEL_UPDATE" != "1" ]; then
    echo "      Pre-staging fresh rpi-update into chroot..."
    mkdir -p "$ROOT_MNT/usr/bin"
    if ! wget -q -O "$ROOT_MNT/usr/bin/rpi-update.fresh" \
            https://raw.githubusercontent.com/Hexxeh/rpi-update/master/rpi-update; then
        rm -f "$ROOT_MNT/usr/bin/rpi-update.fresh"
        echo "      (warning: failed to fetch fresh rpi-update; chroot will install via apt)"
    else
        chmod +x "$ROOT_MNT/usr/bin/rpi-update.fresh"
        # Patch 1: get_long_hash -- short-circuit on a full 40-char SHA.
        # The upstream implementation hits the github API on every call,
        # which fails under qemu-arm TLS. We pass a host-resolved SHA,
        # so the API roundtrip is unnecessary.
        sed -i \
          's|^function get_long_hash {|function get_long_hash {\n\tif [[ "$1" =~ ^[0-9a-f]{40}$ ]]; then echo "$1"; return; fi|' \
          "$ROOT_MNT/usr/bin/rpi-update.fresh"
        # Patch 2: download_rev -- skip the `curl --head` tarball-exists
        # pre-check. It sometimes fails in the chroot even when the real
        # tarball download that follows works fine. We already validated
        # the SHA on the host; if the actual download fails, rpi-update
        # will still error out there.
        sed -i \
          's|^function download_rev {|function download_rev {\n\tif [[ "${FW_REV}" =~ ^[0-9a-f]{40}$ ]]; then SKIP_CHECK=1; fi|' \
          "$ROOT_MNT/usr/bin/rpi-update.fresh"
        sed -i \
          's|if ! eval curl -fs ${CURL_OPTIONS} --output /dev/null --head "${FW_TARBALL_URI}"; then|if [[ "${SKIP_CHECK:-0}" != "1" ]] \&\& ! eval curl -fs ${CURL_OPTIONS} --output /dev/null --head "${FW_TARBALL_URI}"; then|' \
          "$ROOT_MNT/usr/bin/rpi-update.fresh"
        mv -f "$ROOT_MNT/usr/bin/rpi-update.fresh" "$ROOT_MNT/usr/bin/rpi-update"
    fi

    echo "      Resolving rpi-firmware '${RPI_FW_BRANCH}' branch -> commit SHA..."
    RPI_FW_SHA=$(wget -qO- "https://api.github.com/repos/raspberrypi/rpi-firmware/commits/${RPI_FW_BRANCH}" \
        | awk 'BEGIN{h=""} { if (h=="" && $1=="\"sha\":") { h=substr($2,2,40) } } END{print h}')
    if [ -z "$RPI_FW_SHA" ] || [ "${#RPI_FW_SHA}" -ne 40 ]; then
        echo "ERROR: failed to resolve rpi-firmware ${RPI_FW_BRANCH} -> SHA from host" >&2
        exit 1
    fi
    echo "      rpi-firmware ${RPI_FW_BRANCH} -> ${RPI_FW_SHA}"
fi

echo "      Running FPP_Install.sh inside chroot (this takes a while)..."
cat > "$ROOT_MNT/tmp/fpp-chroot-install.sh" <<CHROOT_EOF
#!/bin/bash
set -euxo pipefail

export DEBIAN_FRONTEND=noninteractive
export FPP_INSTALL_YES=1
# Pass the build's OS-version stamp into FPP_Install.sh so /etc/fpp/rfs_version
# inside the image matches the .fppos filename produced by this script.
export FPPIMAGEVER="${OSVERSION}"
export FPPBRANCH="${FPPBRANCH}"

# Configure locale BEFORE any apt activity. Raspbian's base image has
# en_GB.UTF-8 enabled in /etc/locale.gen, and apt-get upgrade triggers the
# locales package's postinst to (re)generate every enabled locale -- which
# wastes minutes regenerating en_GB only for FPP_Install.sh to overwrite it
# with en_US later. Set en_US up front and stick with it (~90% of users).
echo "en_US.UTF-8 UTF-8" > /etc/locale.gen
if [ -f /etc/default/locale ]; then
    echo 'LANG=en_US.UTF-8' > /etc/default/locale
fi
# LANG falls back gracefully if the locale isn't generated yet; LC_ALL does
# not, so we set LC_ALL only after locale-gen below.
export LANG=en_US.UTF-8

# Match the README's manual steps
apt-get update
apt-get -y upgrade
apt-get -y install wget ca-certificates locales
locale-gen en_US.UTF-8 || true
update-locale LANG=en_US.UTF-8 || true
export LC_ALL=en_US.UTF-8

cd /root
/root/FPP_Install.sh --img --yes --branch ${FPPBRANCH} ${INSTALLER_EXTRA_ARGS}

# FPP_Install.sh replaces /etc/resolv.conf with a symlink to
# /run/systemd/resolve/resolv.conf near its end. In our chroot, /run is a
# fresh tmpfs and systemd-resolved never ran -- the symlink dangles and
# DNS breaks for anything after this point (including rpi-update below).
# Restore a working resolv.conf for the remainder of the chroot build;
# step 6 blanks it out before the image is finalized.
rm -f /etc/resolv.conf
cat > /etc/resolv.conf <<'RESOLV_EOF'
nameserver 8.8.8.8
nameserver 1.1.1.1
RESOLV_EOF

#############################################################################
# Kernel update via rpi-update (FPP10 / Debian 13 wants 6.18+).
# Done AS THE VERY LAST STEP of the chroot install, after FPP_Install.sh
# has finished all of its apt activity. Earlier ordering (before the
# installer) meant FPP_Install.sh's apt-get install was silently reinstating
# a 6.12 kernel via package dependencies, shipping images with the wrong
# kernel. Running rpi-update last guarantees nothing in the image build
# can undo it.
# Uses /lib/modules inspection instead of \`uname -r\` (which under qemu
# returns the host kernel, not what's installed in the chroot).
#############################################################################
if [ "${SKIP_KERNEL_UPDATE}" != "1" ]; then
    echo "FPP - Installing latest Pi kernel via rpi-update (SHA ${RPI_FW_SHA})"

    if ! command -v rpi-update >/dev/null 2>&1; then
        apt-get install -y rpi-update
    fi
    # rpi-update's self-update step hits TLS issues under qemu-arm-static
    # (TLS handshake to github often hangs for many minutes then fails).
    # We pre-staged a fresh copy from the host into /usr/bin/rpi-update
    # before entering the chroot, so disable the in-chroot self-update.
    export UPDATE_SELF=0
    # rpi-update defaults BOOT_PATH=/boot with no auto-detection. On modern
    # Raspbian (Debian 12+) the FAT boot partition is mounted at
    # /boot/firmware instead. Without this override, rpi-update writes the
    # kernel images and firmware to /boot inside the rootfs where the
    # bootloader never sees them, and the Pi keeps booting whatever stock
    # kernel apt previously dropped into /boot/firmware/kernel8.img.
    # rpi-update requires BOTH BOOT_PATH and ROOT_PATH set (or neither).
    export BOOT_PATH=/boot/firmware
    export ROOT_PATH=/

    # Snapshot pre-existing kernels so we can identify them as 'old' afterwards
    OLD_KERNELS=\$(ls -1 /lib/modules/ 2>/dev/null || true)

    # Pull latest 6.18+ kernel; PRUNE_MODULES drops other-arch modules,
    # WANT_PI5 ensures Pi5 firmware is included.
    # SKIP_BACKUP: backups are pointless in chroot and bake the wrong
    #   modules path (qemu leaks host's uname -r).
    # SKIP_BOOTLOADER: rpi-eeprom-update needs /proc/device-tree which
    #   doesn't exist in chroot. EEPROM is updated on real hardware later.
    # SKIP_VCLIBS / SKIP_SDK: the 6.18+ ("next") firmware branch no longer
    # ships vc/hardfp or vc/sdk directories (modern Pi userspace uses the
    # in-tree KMS/DRM driver, not the legacy proprietary VC blobs), so the
    # copy step would fail with "No such file or directory".
    PRUNE_MODULES=1 WANT_PI5=1 SKIP_WARNING=1 SKIP_BACKUP=1 SKIP_BOOTLOADER=1 \\
        SKIP_VCLIBS=1 SKIP_SDK=1 \\
        rpi-update ${RPI_FW_SHA}

    # Determine the new kernel: highest-version dir in /lib/modules,
    # excluding the 16K-page-size kernels (some FPP libs are incompatible).
    NEW_KV=\$(ls -1 /lib/modules/ 2>/dev/null | grep -v -- '-16k' | sort -V | tail -n1)
    if [ -z "\$NEW_KV" ]; then
        echo "ERROR: no kernel found in /lib/modules after rpi-update" >&2
        exit 1
    fi
    echo "FPP - New kernel installed: \$NEW_KV"

    # Remove pre-existing kernels (anything that was there before rpi-update
    # ran) and their module/header trees.
    for OLD_KV in \$OLD_KERNELS; do
        if [ "\$OLD_KV" = "\$NEW_KV" ]; then
            continue
        fi
        echo "FPP - Removing old kernel modules: \$OLD_KV"
        rm -rf "/lib/modules/\$OLD_KV"
        rm -f /boot/firmware/*-\${OLD_KV}* 2>/dev/null || true
        rm -rf /usr/lib/linux-image-\${OLD_KV}* 2>/dev/null || true
    done

    # Drop the 16K-page kernel image and any *-16k modules
    rm -f /boot/firmware/kernel_2712.img
    rm -rf /lib/modules/*-16k*

    # Remove kernel-header packages (apt is idempotent if a package is absent;
    # || true covers anything not installed)
    apt-get remove -y --purge --autoremove \\
        linux-headers-rpi-v6 linux-headers-rpi-v7 linux-headers-rpi-v8 2>/dev/null || true
    NEW_KV_BASE="\${NEW_KV%-*}"
    apt-get remove -y --purge --autoremove \\
        "linux-headers-\${NEW_KV_BASE}-v6" \\
        "linux-headers-\${NEW_KV_BASE}-v7" \\
        "linux-headers-\${NEW_KV_BASE}-v8" 2>/dev/null || true
    rm -rf /usr/src/linux-*
fi

# Finalization (mirrors SD/README.RaspberryPi post-install cleanup)
apt-get clean
journalctl --flush --rotate --vacuum-time=1s || true
rm -rf /usr/src/linux-headers-*
rm -rf /home/pi
rm -f /home/fpp/.bash_history
rm -f /home/fpp/media/settings
rm -rf /home/fpp/media/config/backups/*
rm -f /home/fpp/media/config/*
rm -f /home/fpp/media/logs/*
rm -f /home/fpp/media/tmp/*
rm -f /var/log/* 2>/dev/null || true
rm -f /var/swap
rm -f /etc/ssh/*key*
rm -f /root/.bash_history
# machine-id is FPP's last-resort fallback UUID source (used only when no
# hardware serial exists), so it must be unique per device. systemd seeds it from
# /var/lib/dbus/machine-id when that's a real file, so a baked-in value would be
# shared by every device flashed from the image; symlink the dbus id and reset
# /etc/machine-id so a fresh, unique one is generated on each first boot.
rm -f /var/lib/dbus/machine-id
ln -sf /etc/machine-id /var/lib/dbus/machine-id
echo "uninitialized" > /etc/machine-id
CHROOT_EOF
chmod +x "$ROOT_MNT/tmp/fpp-chroot-install.sh"

chroot "$ROOT_MNT" /tmp/fpp-chroot-install.sh

rm -f "$ROOT_MNT/tmp/fpp-chroot-install.sh"

#############################################################################
# 5b. Capture the populated ccache so the workflow can attach it to the
# release tag. On upgrade, devices download ccache-<suffix>.tar.gz for the
# matching release and merge it in to make on-device rebuilds fast. Stored in
# XDG layout so it extracts straight into /root/.cache/ccache.
#############################################################################
if [ -d "$ROOT_MNT/root/.cache/ccache" ]; then
    echo "Creating ccache tarball: ccache-${PLATFORM_SUFFIX}.tar.gz"
    tar -czf "$OUTPUT_DIR/ccache-${PLATFORM_SUFFIX}.tar.gz" \
        -C "$ROOT_MNT/root/.cache/ccache" .
fi

#############################################################################
# 6. Mark first-boot expand + strip qemu binary
#############################################################################
echo "[6/8] Marking first-boot expand and stripping build artifacts..."
touch "$ROOT_MNT/boot/firmware/fpp_expand_rootfs"
rm -f "$ROOT_MNT/usr/bin/$QEMU_BIN"
# Restore the /etc/resolv.conf -> systemd-resolved symlink for the booted
# image. We injected a real resolv.conf above for the chroot's apt/rpi-update;
# leaving it as a static (now empty) file means anything that reads
# /etc/resolv.conf directly (c-ares, node, plugins like PulseMesh) gets no
# nameservers. getaddrinfo() still works via nss-resolve in nsswitch.conf,
# which masks the problem (ping/curl/php resolve fine) -- hence GitHub #2675.
# systemd-resolved does NOT create this symlink itself, so we must. It matches
# FPP_Install.sh's finalize_image_post_build(). The target is resolved at boot
# when systemd-resolved populates /run/systemd/resolve/resolv.conf.
rm -f "$ROOT_MNT/etc/resolv.conf"
ln -sf /run/systemd/resolve/resolv.conf "$ROOT_MNT/etc/resolv.conf"

#############################################################################
# 7. Drop chroot bind mounts BEFORE mksquashfs.
# Otherwise mksquashfs walks the host's /proc, /sys, /dev, /run and either
# fails on unreadable files or bakes host kernel state into the .fppos.
# The first two MOUNTS entries are ROOT_MNT and ROOT_MNT/boot/firmware --
# keep those, drop the rest (chroot binds and tmpfs).
#############################################################################
echo "[7/8] Dropping chroot bind mounts before squashfs/zerofree..."
for (( i=${#MOUNTS[@]}-1; i>=2; i-- )); do
    umount "${MOUNTS[$i]}" || umount -l "${MOUNTS[$i]}" || true
done
# Trim MOUNTS to just the rootfs + boot mounts that remain.
MOUNTS=("${MOUNTS[0]}" "${MOUNTS[1]}")

#############################################################################
# 7b. (Optional) Build .fppos squashfs from the cleanly-mounted rootfs.
#############################################################################
if [ "$SKIP_FPPOS" != "1" ]; then
    echo "[7b/8] Building .fppos squashfs (this takes a LONG time)..."
    FPPOS="$OUTPUT_DIR/${PLATFORM_SUFFIX}-${VERSION}_${OSVERSION}.fppos"
    rm -f "$FPPOS"
    mksquashfs "$ROOT_MNT" "$FPPOS" -b 512K -comp xz -noappend
else
    echo "[7b/8] Skipping .fppos generation (SKIP_FPPOS=1)"
    FPPOS=""
fi

#############################################################################
# 8. Unmount remaining, zerofree, package
#############################################################################
echo "[8/8] Unmounting, zeroing free blocks, and packaging..."
# Unmount in reverse order (just rootfs + boot now)
for (( i=${#MOUNTS[@]}-1; i>=0; i-- )); do
    umount "${MOUNTS[$i]}"
done
MOUNTS=()

zerofree -v "$ROOTP"

losetup -d "$LOOPDEV"
LOOPDEV=""

OUT_IMG="FPP-v${VERSION}-${PLATFORM_SUFFIX}.img"
mv -f "$WORK_IMG" "$OUTPUT_DIR/$OUT_IMG"
if [ "$SKIP_ZIP" != "1" ]; then
    ( cd "$OUTPUT_DIR" && rm -f "${OUT_IMG}.zip" && zip -9 "${OUT_IMG}.zip" "$OUT_IMG" )
fi

cat <<EOF

============================================================
Build complete.

EOF
if [ "$SKIP_ZIP" != "1" ]; then
    echo "  Flashable image : $OUTPUT_DIR/${OUT_IMG}.zip"
fi
echo "  Raw image       : $OUTPUT_DIR/${OUT_IMG}"
if [ -n "$FPPOS" ]; then
    echo "  OS update image : $FPPOS"
fi
echo "============================================================"

if [ "$KEEP_WORK" != "1" ]; then
    echo "Cleaning work dir (set --keep-work to retain)..."
    rm -f "$WORK_DIR/$WORK_IMG"
fi
