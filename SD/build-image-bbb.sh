#!/bin/bash
#############################################################################
# build-image-bbb.sh - Automated FPP image builder for BeagleBone Black
# (armhf, am335x).
#
# Modeled on build-image-bb64.sh. Key differences:
#   - 32-bit armhf (uses qemu-arm-static; slow when host is x86_64 or arm64).
#   - Installs FPP's custom kernel from the fpp-linux-kernel github repo
#     (the rcn-ee base image ships a stock 6.18 kernel; FPP needs its
#     PRU/cape patches).
#   - Blacklists bluetooth modules (needed for BBGW / BBBW variants).
#   - fpp_expand_rootfs marker lives in /boot/ (rootfs), not /boot/firmware/
#     (FAT) -- different from Pi/BB64.
#
# Prereqs (Debian/Ubuntu):
#   apt-get install qemu-user-static binfmt-support parted dosfstools \
#       zerofree squashfs-tools zip xz-utils wget ca-certificates e2fsprogs
#
# IMPORTANT: qemu-arm-static binfmt must be registered. On arm64/x86_64 hosts
# where the qemu-user-static package skips registering qemu-arm, run:
#   sudo update-binfmts --enable qemu-arm
# (or the manual /proc/sys/fs/binfmt_misc/register write -- see Pi script
# header for the exact line).
#############################################################################

set -euo pipefail

#############################################################################
# Defaults (overridable via args or env)
#############################################################################
VERSION="${VERSION:-}"
OSVERSION="${OSVERSION:-$(date +%Y-%m)}"
FPPBRANCH="${FPPBRANCH:-master}"
# rcn-ee Debian armhf base v6.18 base image (am335x).  Listing at
# https://rcn-ee.net/rootfs/debian-armhf-13-base-v6.18/${BASE_IMAGE_DATE}/
BASE_IMAGE_DATE="${BASE_IMAGE_DATE:-2026-06-03}"
BASE_IMAGE_DEBVER="${BASE_IMAGE_DEBVER:-13.5}"
BASE_IMAGE_KERNEL="${BASE_IMAGE_KERNEL:-v6.18}"
BASE_IMAGE_SIZE="${BASE_IMAGE_SIZE:-4gb}"
BASE_IMAGE_NAME="${BASE_IMAGE_NAME:-am335x-debian-${BASE_IMAGE_DEBVER}-base-${BASE_IMAGE_KERNEL}-armhf-${BASE_IMAGE_DATE}-${BASE_IMAGE_SIZE}.img.xz}"
BASE_IMAGE_URL="${BASE_IMAGE_URL:-}"
BASE_IMAGE_SHA256="${BASE_IMAGE_SHA256:-}"
IMG_SIZE_MB="${IMG_SIZE_MB:-7000}"

# FPP-patched kernel deb (the rcn-ee base ships a stock kernel; FPP requires
# its PRU/cape patches). Override env vars to point at a different build.
FPP_KERNEL_VER="${FPP_KERNEL_VER:-6.18.8-fpp18_1}"
FPP_KERNEL_URL="${FPP_KERNEL_URL:-https://github.com/FalconChristmas/fpp-linux-kernel/raw/master/debs/linux-image-${FPP_KERNEL_VER}_armhf.deb}"
SKIP_KERNEL_UPDATE="${SKIP_KERNEL_UPDATE:-0}"

WORK_DIR="${WORK_DIR:-$(pwd)/build}"
OUTPUT_DIR="${OUTPUT_DIR:-$(pwd)/output}"
KEEP_WORK="${KEEP_WORK:-0}"
SKIP_FPPOS="${SKIP_FPPOS:-0}"
SKIP_ZIP="${SKIP_ZIP:-0}"
USE_LOCAL_SRC="${USE_LOCAL_SRC:-0}"
FPP_SRC_DIR="${FPP_SRC_DIR:-$(readlink -f "$(dirname "$(readlink -f "$0")")/..")}"

PLATFORM_SUFFIX="BBB"
QEMU_BIN="qemu-arm-static"

usage() {
    cat <<EOF
Usage: $0 [options]

Options:
  --version VER            FPP version string (required, e.g. 10.0)
  --os-version VER         OS version tag for .fppos (default: YYYY-MM)
  --branch BRANCH          FPP git branch to install (default: master)
  --base-image-url URL     Override base image URL (full path to .img.xz)
  --base-image-date DATE   rcn-ee build date (default: ${BASE_IMAGE_DATE})
  --base-image-name NAME   rcn-ee image filename (default derived from date)
  --base-image-sha256 HEX  Optional sha256 of the .img.xz to verify
  --img-size-mb N          Raw output image size in MiB (default: ${IMG_SIZE_MB})
  --fpp-kernel-ver VER     FPP kernel version (default: ${FPP_KERNEL_VER})
  --fpp-kernel-url URL     Override FPP kernel .deb URL
  --skip-kernel-update     Don't install the FPP-patched kernel
  --work-dir DIR           Scratch dir (default: ./build)
  --output-dir DIR         Artifact dir (default: ./output)
  --skip-fppos             Do not produce .fppos squashfs
  --skip-zip               Do not zip the raw .img (faster iteration when
                           flashing the raw image directly to an SD card)
  --use-local-src          Seed /opt/fpp from local FPP working tree
  --fpp-src-dir DIR        Path to local FPP checkout
  --keep-work              Keep working directory on success
  -h, --help               Show this help
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --version)            VERSION="$2"; shift 2 ;;
        --os-version)         OSVERSION="$2"; shift 2 ;;
        --branch)             FPPBRANCH="$2"; shift 2 ;;
        --base-image-url)     BASE_IMAGE_URL="$2"; shift 2 ;;
        --base-image-date)    BASE_IMAGE_DATE="$2"; shift 2 ;;
        --base-image-name)    BASE_IMAGE_NAME="$2"; shift 2 ;;
        --base-image-sha256)  BASE_IMAGE_SHA256="$2"; shift 2 ;;
        --img-size-mb)        IMG_SIZE_MB="$2"; shift 2 ;;
        --fpp-kernel-ver)     FPP_KERNEL_VER="$2"; shift 2 ;;
        --fpp-kernel-url)     FPP_KERNEL_URL="$2"; shift 2 ;;
        --skip-kernel-update) SKIP_KERNEL_UPDATE=1; shift ;;
        --work-dir)           WORK_DIR="$2"; shift 2 ;;
        --output-dir)         OUTPUT_DIR="$2"; shift 2 ;;
        --skip-fppos)         SKIP_FPPOS=1; shift ;;
        --skip-zip)           SKIP_ZIP=1; shift ;;
        --use-local-src)      USE_LOCAL_SRC=1; shift ;;
        --fpp-src-dir)        FPP_SRC_DIR="$(readlink -f "$2")"; shift 2 ;;
        --keep-work)          KEEP_WORK=1; shift ;;
        -h|--help)            usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

#############################################################################
# Validate + derive
#############################################################################
if [ -z "$VERSION" ]; then
    echo "--version is required (e.g. --version 10.0)" >&2
    exit 2
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root (needs losetup, mount, chroot)." >&2
    exit 1
fi

if [ -z "$BASE_IMAGE_URL" ]; then
    BASE_IMAGE_URL="https://rcn-ee.net/rootfs/debian-armhf-13-base-${BASE_IMAGE_KERNEL}/${BASE_IMAGE_DATE}/${BASE_IMAGE_NAME}"
fi

LOCAL_INSTALLER="${FPP_SRC_DIR}/SD/FPP_Install.sh"
if [ ! -f "$LOCAL_INSTALLER" ]; then
    echo "Local FPP_Install.sh not found at $LOCAL_INSTALLER" >&2
    exit 1
fi

if [ "$USE_LOCAL_SRC" = "1" ] && [ ! -d "$FPP_SRC_DIR/.git" ]; then
    echo "--use-local-src requires $FPP_SRC_DIR to be a git checkout" >&2
    exit 1
fi

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
FPP Image Build (BBB / am335x armhf)
  FPP version  : $VERSION
  OS version   : $OSVERSION
  Branch       : $FPPBRANCH
  Base image   : $BASE_IMAGE_URL
  FPP kernel   : $( [ "$SKIP_KERNEL_UPDATE" = "1" ] && echo "SKIPPED (--skip-kernel-update)" || echo "$FPP_KERNEL_VER ($FPP_KERNEL_URL)" )
  Local installer: $LOCAL_INSTALLER
  Local src    : $( [ "$USE_LOCAL_SRC" = "1" ] && echo "$FPP_SRC_DIR (seeded into /opt/fpp)" || echo "(not used; installer clones from github)" )
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
# 1b. Pre-fetch FPP kernel deb from host (TLS in chroot under qemu-arm is
#     unreliable -- same family of issues that broke rpi-update self-update).
#############################################################################
FPP_KERNEL_DEB=""
if [ "$SKIP_KERNEL_UPDATE" != "1" ]; then
    FPP_KERNEL_DEB="$(basename "$FPP_KERNEL_URL")"
    if [ ! -f "$WORK_DIR/$FPP_KERNEL_DEB" ]; then
        echo "      Pre-fetching FPP kernel deb on host..."
        wget -O "${FPP_KERNEL_DEB}.part" "$FPP_KERNEL_URL"
        mv "${FPP_KERNEL_DEB}.part" "$FPP_KERNEL_DEB"
    else
        echo "      FPP kernel deb already cached: $FPP_KERNEL_DEB"
    fi
fi

#############################################################################
# 2. Decompress + grow image file
#############################################################################
WORK_IMG="fpp-${PLATFORM_SUFFIX}-v${VERSION}.img"

echo "[2/8] Decompressing and preparing working image..."
rm -f "$WORK_IMG"
xz -dc "$BASE_XZ" > "$WORK_IMG"
CURRENT_BYTES=$(stat -c%s "$WORK_IMG")
TARGET_BYTES=$((IMG_SIZE_MB * 1024 * 1024))
if [ "$TARGET_BYTES" -lt "$CURRENT_BYTES" ]; then
    echo "ERROR: --img-size-mb (${IMG_SIZE_MB}) is smaller than the base" >&2
    echo "       image (~$((CURRENT_BYTES/1024/1024))M). Bump --img-size-mb." >&2
    exit 1
fi
truncate -s "${IMG_SIZE_MB}M" "$WORK_IMG"

#############################################################################
# 3. Grow rootfs partition (p3 on BB layout) + attach loop + resize FS
#############################################################################
echo "[3/8] Growing root partition (p3) to fill image..."
parted -s "$WORK_IMG" resizepart 3 100%

LOOPDEV="$(losetup --show -f -P --direct-io=on "$WORK_IMG" 2>/dev/null)" \
    || LOOPDEV="$(losetup --show -f -P "$WORK_IMG")"
BOOTP="${LOOPDEV}p1"
ROOTP="${LOOPDEV}p3"

for _ in 1 2 3 4 5; do
    [ -b "$BOOTP" ] && [ -b "$ROOTP" ] && break
    sleep 1
done
[ -b "$ROOTP" ] || { echo "Partition device $ROOTP not found" >&2; exit 1; }

echo "      fsck + resize2fs on $ROOTP"
e2fsck -fy "$ROOTP" || true
resize2fs "$ROOTP"

#############################################################################
# 4. Mount + apply pre-boot configuration
#############################################################################
echo "[4/8] Mounting and applying pre-boot configuration..."
ROOT_MNT="$(mktemp -d -t fpp-rootfs.XXXXXX)"
mount "$ROOTP" "$ROOT_MNT"
MOUNTS+=("$ROOT_MNT")
mkdir -p "$ROOT_MNT/boot/firmware"
mount "$BOOTP" "$ROOT_MNT/boot/firmware"
MOUNTS+=("$ROOT_MNT/boot/firmware")

# Disable rcn-ee's first-boot growpart.
rm -f "$ROOT_MNT/etc/bbb.io/growpart"

# Replace stock network configs with FPP's setup.
NET_SRC="${FPP_SRC_DIR}/etc/systemd/network/50-default.network"
if [ -f "$NET_SRC" ]; then
    install -m 0644 "$NET_SRC" "$ROOT_MNT/etc/systemd/network/50-default.network"
fi
rm -f "$ROOT_MNT"/etc/systemd/network/eth0* \
      "$ROOT_MNT"/etc/systemd/network/mlan* \
      "$ROOT_MNT"/etc/systemd/network/wlan*

# sysconf.txt: rcn-ee first-boot reads this to create the user.
{
    echo "user_name=fpp"
    echo "user_password=falcon"
} >> "$ROOT_MNT/boot/firmware/sysconf.txt"

# udev rule for the ASIX USB-Gigabit adapter.
mkdir -p "$ROOT_MNT/etc/udev/rules.d"
cat > "$ROOT_MNT/etc/udev/rules.d/99-asix.rules" <<'UDEV_EOF'
ACTION=="add", SUBSYSTEM=="usb", ATTR{idVendor}=="0b95", ATTR{idProduct}=="1790", ATTR{bConfigurationValue}!="1", ATTR{bConfigurationValue}="1"
UDEV_EOF

# Bluetooth blacklist for BBGW / BBBW (per README.BBB).
mkdir -p "$ROOT_MNT/etc/modprobe.d"
cat > "$ROOT_MNT/etc/modprobe.d/blacklist-bluetooth.conf" <<'BT_EOF'
blacklist bluetooth
blacklist hci_uart
blacklist bnep
BT_EOF

# Stage the FPP kernel deb where the chroot can find it.
if [ -n "$FPP_KERNEL_DEB" ]; then
    install -m 0644 "$WORK_DIR/$FPP_KERNEL_DEB" "$ROOT_MNT/root/$FPP_KERNEL_DEB"
fi

# Writeable resolv.conf for apt/curl in chroot (rcn-ee likely symlinks it).
rm -f "$ROOT_MNT/etc/resolv.conf"
cp -L /etc/resolv.conf "$ROOT_MNT/etc/resolv.conf"

# qemu-user-static for armhf execution.
install -m 0755 "$(command -v "$QEMU_BIN")" "$ROOT_MNT/usr/bin/$QEMU_BIN"

# Bind mounts; /run gets a fresh tmpfs (see Pi script for rationale).
for d in dev dev/pts proc sys; do
    mkdir -p "$ROOT_MNT/$d"
    mount --bind "/$d" "$ROOT_MNT/$d"
    MOUNTS+=("$ROOT_MNT/$d")
done
mkdir -p "$ROOT_MNT/run"
mount -t tmpfs -o mode=755 tmpfs "$ROOT_MNT/run"
MOUNTS+=("$ROOT_MNT/run")

#############################################################################
# 5. Run BBB prep + FPP_Install.sh inside chroot
#############################################################################
echo "[5/8] Staging local FPP_Install.sh into chroot..."
install -m 0700 "$LOCAL_INSTALLER" "$ROOT_MNT/root/FPP_Install.sh"

INSTALLER_EXTRA_ARGS=""
if [ "$USE_LOCAL_SRC" = "1" ]; then
    echo "      Seeding /opt/fpp from $FPP_SRC_DIR ..."
    mkdir -p "$ROOT_MNT/opt/fpp"
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

echo "      Running BBB prep + FPP_Install.sh inside chroot..."
cat > "$ROOT_MNT/tmp/fpp-chroot-install.sh" <<CHROOT_EOF
#!/bin/bash
set -euxo pipefail

export DEBIAN_FRONTEND=noninteractive
export FPP_INSTALL_YES=1
# /sys/class/leds/beaglebone:* autodetection doesn't work in chroot.
export FPPPLATFORM_OVERRIDE="BeagleBone Black"
export FPPIMAGEVER="${OSVERSION}"
export FPPBRANCH="${FPPBRANCH}"

# Locale up front to avoid en_GB regen during apt upgrade.
echo "en_US.UTF-8 UTF-8" > /etc/locale.gen
if [ -f /etc/default/locale ]; then
    echo 'LANG=en_US.UTF-8' > /etc/default/locale
fi
export LANG=en_US.UTF-8

apt-get update
apt-get -y upgrade
apt-get -y install wget ca-certificates locales
locale-gen en_US.UTF-8 || true
update-locale LANG=en_US.UTF-8 || true
export LC_ALL=en_US.UTF-8

# Strip BBB-specific bloat per README.BBB. Filter to installed packages.
# cockpit* is the rcn-ee web admin console on :9090 -- pointless for FPP (web
# UI only, no desktop) and only bound to localhost, so purge it.
BB_PURGE_CANDIDATES="bb-code-server libstd-rust-dev rustc docker.io containerd nginx nginx-full nginx-common bb-u-boot-am57xx-evm firmware-nvidia-graphics firmware-intel-graphics cockpit cockpit-ws cockpit-bridge cockpit-system cockpit-packagekit"
# More base-image dev/admin bloat unused by FPP (the dpkg -s filter below skips
# any not present): the full llvm/clang dev stack (~530MB) -- FPP only needs
# clang-format, which keeps its libllvm19/libclang-cpp19 runtime -- plus the
# leftover docker CLI tooling (docker.io/containerd are purged above).
# NOTE: no arm-linux-gnueabihf cross compiler here -- on the 32-bit BBB image
# that is the native toolchain.
# (mesa-vulkan-drivers / samba-ad-provision get pulled back in as deps by
# FPP_Install, so they are purged AFTER it runs -- see POST_PURGE below.)
BB_PURGE_CANDIDATES="\$BB_PURGE_CANDIDATES llvm-19 llvm-19-dev llvm-19-runtime llvm-19-linker-tools clang-19 clang-tools-19 libclang-rt-19-dev libclang-common-19-dev docker-cli docker-buildx docker-compose"
BB_PURGE_INSTALLED=""
for pkg in \$BB_PURGE_CANDIDATES; do
    if dpkg -s "\$pkg" >/dev/null 2>&1; then
        BB_PURGE_INSTALLED="\$BB_PURGE_INSTALLED \$pkg"
    fi
done
if [ -n "\$BB_PURGE_INSTALLED" ]; then
    apt-get remove -y --autoremove --purge \$BB_PURGE_INSTALLED
fi

rm -rf /opt/source/dtb-5* /opt/source/dtb-6.1.x* /opt/source/dtb-6.6.x* /opt/source/dtb-6.12.x /opt/source/dtb-6.16.x /opt/source/dtb-6.17.x /opt/source/spi* /opt/source/py*

cd /root
/root/FPP_Install.sh --img --yes --branch ${FPPBRANCH} ${INSTALLER_EXTRA_ARGS}

# Purge packages that FPP_Install pulls back in as (recommended) deps but that
# a headless FPP has no use for. Done here, after the install, because the
# pre-install BB_PURGE above runs before these get dragged in. dpkg -s filter
# keeps it safe if a name isn't present.
#   mesa-vulkan-drivers -- Vulkan ICD; FPP video is GStreamer/GL, no Vulkan
#   samba-ad-provision  -- Samba AD domain-controller tooling; FPP only file-shares
POST_PURGE="mesa-vulkan-drivers samba-ad-provision"
POST_PURGE_INSTALLED=""
for pkg in \$POST_PURGE; do
    if dpkg -s "\$pkg" >/dev/null 2>&1; then
        POST_PURGE_INSTALLED="\$POST_PURGE_INSTALLED \$pkg"
    fi
done
if [ -n "\$POST_PURGE_INSTALLED" ]; then
    apt-get remove -y --autoremove --purge \$POST_PURGE_INSTALLED
fi

# FPP_Install.sh replaces /etc/resolv.conf with a symlink to
# /run/systemd/resolve/resolv.conf near its end. In our chroot /run is a
# fresh tmpfs and systemd-resolved never ran -- the symlink dangles and
# DNS breaks for anything that comes after. Restore working DNS for the
# apt-get operations in the kernel step below; step 6 blanks this out
# before the image is finalized.
rm -f /etc/resolv.conf
cat > /etc/resolv.conf <<'RESOLV_EOF'
nameserver 8.8.8.8
nameserver 1.1.1.1
RESOLV_EOF

#############################################################################
# Install FPP-patched kernel.
# Done AS THE VERY LAST STEP of the chroot install, after FPP_Install.sh
# has finished all of its apt activity. Earlier ordering (before the
# installer) let FPP_Install.sh's apt-get install silently reinstate a
# stock kernel via package dependencies (bbb.io-kernel-tasks et al),
# shipping images with the wrong kernel.
# uname -r returns the host kernel under qemu, so we identify the OLD
# kernel by snapshotting /lib/modules instead.
#############################################################################
if [ "${SKIP_KERNEL_UPDATE}" != "1" ] && [ -f "/root/${FPP_KERNEL_DEB}" ]; then
    echo "FPP - Installing FPP-patched kernel ${FPP_KERNEL_VER}"
    OLD_KERNELS=\$(ls -1 /lib/modules/ 2>/dev/null || true)

    dpkg -i "/root/${FPP_KERNEL_DEB}"
    rm -f "/root/${FPP_KERNEL_DEB}"

    # bbb.io-kernel-tasks pulls in stock kernels via apt; remove it before
    # we purge old kernels so they don't get reinstalled.
    apt-get remove -y --purge --autoremove bbb.io-kernel-tasks 2>/dev/null || true

    NEW_KV=\$(ls -1 /lib/modules/ 2>/dev/null | sort -V | tail -n1)
    if [ -z "\$NEW_KV" ]; then
        echo "ERROR: no kernel in /lib/modules after dpkg -i" >&2
        exit 1
    fi
    echo "FPP - New kernel: \$NEW_KV"

    for OLD_KV in \$OLD_KERNELS; do
        [ "\$OLD_KV" = "\$NEW_KV" ] && continue
        echo "FPP - Removing old kernel modules: \$OLD_KV"
        rm -rf "/lib/modules/\$OLD_KV"
        rm -rf "/boot/dtbs/\$OLD_KV"
        # Best-effort apt removal of stock linux-image / -headers for OLD_KV
        apt-get remove -y --purge --autoremove "linux-image-\$OLD_KV" 2>/dev/null || true
        apt-get remove -y --purge --autoremove "linux-headers-\$OLD_KV" 2>/dev/null || true
    done
    rm -rf /boot/initrd.img*
    
    # Remove the blacklist of the rtw88 drivers as we don't have the out-of-tree drivers
    # as part of our kernel
    rm -f /etc/modprobe.d/rtw88.conf
fi

# Finalization (mirrors SD/README.BBB post-install cleanup)
apt-get clean
journalctl --flush --rotate --vacuum-time=1s || true
rm -f /home/fpp/.bash_history
rm -f /home/fpp/media/settings
rm -rf /home/fpp/media/config/backups/*
rm -f /home/fpp/media/config/*
rm -f /home/fpp/media/logs/*
rm -rf /home/fpp/media/tmp/*
rm -rf /home/fpp/media/crashes/*
rm -rf /home/fpp/media/upload/*
rm -f /var/log/* 2>/dev/null || true
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
# 6. Mark first-boot expand + strip qemu binary.
# IMPORTANT: BBB looks for fpp_expand_rootfs in /boot/ (rootfs path), not
# /boot/firmware/ -- different from Pi/BB64.
#############################################################################
echo "[6/8] Marking first-boot expand and stripping build artifacts..."
touch "$ROOT_MNT/boot/fpp_expand_rootfs"
rm -f "$ROOT_MNT/usr/bin/$QEMU_BIN"
# Restore the /etc/resolv.conf -> systemd-resolved symlink for the booted
# image. Leaving it as a static (empty) file means anything that reads
# /etc/resolv.conf directly gets no nameservers; getaddrinfo() still works via
# nss-resolve, masking the problem (GitHub #2675). systemd-resolved does NOT
# create this symlink itself. Matches FPP_Install.sh's finalize_image_post_build().
rm -f "$ROOT_MNT/etc/resolv.conf"
ln -sf /run/systemd/resolve/resolv.conf "$ROOT_MNT/etc/resolv.conf"

#############################################################################
# 7. Drop chroot binds before squashfs
#############################################################################
echo "[7/8] Dropping chroot bind mounts before squashfs/zerofree..."
for (( i=${#MOUNTS[@]}-1; i>=2; i-- )); do
    umount "${MOUNTS[$i]}" || umount -l "${MOUNTS[$i]}" || true
done
MOUNTS=("${MOUNTS[0]}" "${MOUNTS[1]}")

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
