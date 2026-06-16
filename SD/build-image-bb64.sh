#!/bin/bash
#############################################################################
# build-image-bb64.sh - Automated FPP image builder for BeagleBone 64
# (PocketBeagle 2 / BeagleBone AI-64, arm64).
#
# Mirrors SD/build-image-pi.sh in structure. Differences:
#   - Base image is the rcn-ee Debian arm64 IoT rootfs (already on 6.18 kernel,
#     so no rpi-update step is needed).
#   - BB partition layout uses 3 partitions: p1=FAT boot, p2=unused, p3=ext4
#     rootfs (Pi only has 2).
#   - Pre-boot tweaks differ: disable growpart, replace network config, write
#     sysconf.txt for first-boot user creation, drop udev rule for the
#     ASIX USB-Gigabit adapter.
#   - A pile of stock bloat is removed pre-FPP_Install (node-red, code-server,
#     unused u-boot variants, rust toolchain, docker, nginx).
#
# Prereqs (Debian/Ubuntu):
#   apt-get install qemu-user-static binfmt-support parted dosfstools \
#       zerofree squashfs-tools zip xz-utils wget ca-certificates e2fsprogs
#
# On an arm64 host this runs natively (no qemu emulation).
#############################################################################

set -euo pipefail

#############################################################################
# Defaults (overridable via args or env)
#############################################################################
VERSION="${VERSION:-}"
OSVERSION="${OSVERSION:-$(date +%Y-%m)}"
FPPBRANCH="${FPPBRANCH:-master}"
# Default base image: latest pocketbeagle2 IoT arm64 build referenced in
# SD/README.BB64. The directory listing lives at
# https://rcn-ee.net/rootfs/debian-arm64-13-iot-v6.18-k3/${BASE_IMAGE_DATE}/
# When upstream rev'd, update BASE_IMAGE_DATE or pass --base-image-url.
BASE_IMAGE_DATE="${BASE_IMAGE_DATE:-2026-06-03}"
BASE_IMAGE_DEBVER="${BASE_IMAGE_DEBVER:-13.5}"
BASE_IMAGE_KERNEL="${BASE_IMAGE_KERNEL:-v6.18-k3}"
BASE_IMAGE_SIZE="${BASE_IMAGE_SIZE:-8gb}"
BASE_IMAGE_NAME="${BASE_IMAGE_NAME:-pocketbeagle2-debian-${BASE_IMAGE_DEBVER}-iot-${BASE_IMAGE_KERNEL}-arm64-${BASE_IMAGE_DATE}-${BASE_IMAGE_SIZE}.img.xz}"
BASE_IMAGE_URL="${BASE_IMAGE_URL:-}"
BASE_IMAGE_SHA256="${BASE_IMAGE_SHA256:-}"
IMG_SIZE_MB="${IMG_SIZE_MB:-0}"      # 0 = use base image size as-is
WORK_DIR="${WORK_DIR:-$(pwd)/build}"
OUTPUT_DIR="${OUTPUT_DIR:-$(pwd)/output}"
KEEP_WORK="${KEEP_WORK:-0}"
SKIP_FPPOS="${SKIP_FPPOS:-0}"
SKIP_ZIP="${SKIP_ZIP:-0}"
USE_LOCAL_SRC="${USE_LOCAL_SRC:-0}"
FPP_SRC_DIR="${FPP_SRC_DIR:-$(readlink -f "$(dirname "$(readlink -f "$0")")/..")}"

PLATFORM_SUFFIX="BB64"
QEMU_BIN="qemu-aarch64-static"

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
  --img-size-mb N          Raw output image size in MiB (default: base image size)
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
        --version)           VERSION="$2"; shift 2 ;;
        --os-version)        OSVERSION="$2"; shift 2 ;;
        --branch)            FPPBRANCH="$2"; shift 2 ;;
        --base-image-url)    BASE_IMAGE_URL="$2"; shift 2 ;;
        --base-image-date)   BASE_IMAGE_DATE="$2"; shift 2 ;;
        --base-image-name)   BASE_IMAGE_NAME="$2"; shift 2 ;;
        --base-image-sha256) BASE_IMAGE_SHA256="$2"; shift 2 ;;
        --img-size-mb)       IMG_SIZE_MB="$2"; shift 2 ;;
        --work-dir)          WORK_DIR="$2"; shift 2 ;;
        --output-dir)        OUTPUT_DIR="$2"; shift 2 ;;
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
if [ -z "$VERSION" ]; then
    echo "--version is required (e.g. --version 10.0)" >&2
    exit 2
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root (needs losetup, mount, chroot)." >&2
    exit 1
fi

if [ -z "$BASE_IMAGE_URL" ]; then
    BASE_IMAGE_URL="https://rcn-ee.net/rootfs/debian-arm64-13-iot-${BASE_IMAGE_KERNEL}/${BASE_IMAGE_DATE}/${BASE_IMAGE_NAME}"
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
FPP Image Build (BB64 / PocketBeagle 2)
  FPP version  : $VERSION
  OS version   : $OSVERSION
  Branch       : $FPPBRANCH
  Base image   : $BASE_IMAGE_URL
  Local installer: $LOCAL_INSTALLER
  Local src    : $( [ "$USE_LOCAL_SRC" = "1" ] && echo "$FPP_SRC_DIR (seeded into /opt/fpp)" || echo "(not used; installer clones from github)" )
  Image size   : $( [ "$IMG_SIZE_MB" -gt 0 ] && echo "${IMG_SIZE_MB} MiB" || echo "(base image size)" )
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
# 2. Decompress + grow image file
#############################################################################
WORK_IMG="fpp-${PLATFORM_SUFFIX}-v${VERSION}.img"

echo "[2/8] Decompressing and preparing working image..."
rm -f "$WORK_IMG"
xz -dc "$BASE_XZ" > "$WORK_IMG"
# When IMG_SIZE_MB is 0 (the default), keep the base image size as-is.
# Otherwise, grow (never shrink) the image to the requested size.
CURRENT_BYTES=$(stat -c%s "$WORK_IMG")
if [ "$IMG_SIZE_MB" -gt 0 ]; then
    TARGET_BYTES=$((IMG_SIZE_MB * 1024 * 1024))
    if [ "$TARGET_BYTES" -lt "$CURRENT_BYTES" ]; then
        echo "ERROR: --img-size-mb (${IMG_SIZE_MB}) is smaller than the base" >&2
        echo "       image (${CURRENT_BYTES} bytes / ~$((CURRENT_BYTES/1024/1024))M)." >&2
        echo "       Pass a larger --img-size-mb to avoid truncating the partition table." >&2
        exit 1
    fi
    truncate -s "${IMG_SIZE_MB}M" "$WORK_IMG"
else
    IMG_SIZE_MB=$((CURRENT_BYTES / 1024 / 1024))
fi

#############################################################################
# 3. Grow root partition (p3 on BB) + attach loop device + resize FS
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

# Labels: BB rootfs commonly has its own label; we don't override here
# because /etc/fstab on the rcn-ee image typically references PARTUUID, not
# LABEL. fatlabel on the boot partition is left as-is for the same reason.

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

# Disable rcn-ee's first-boot growpart (FPP manages its own expand via the
# fpp_expand_rootfs marker touched in step 6).
rm -f "$ROOT_MNT/etc/bbb.io/growpart"

# Replace stock network configs with FPP's eth0/usb0 setup. Fetched from
# the local checkout so iteration on it doesn't require pushing to github.
NET_SRC="${FPP_SRC_DIR}/etc/systemd/network/50-default.network"
if [ -f "$NET_SRC" ]; then
    install -m 0644 "$NET_SRC" "$ROOT_MNT/etc/systemd/network/50-default.network"
else
    echo "      (warning: $NET_SRC missing; skipping network config drop)"
fi
rm -f "$ROOT_MNT"/etc/systemd/network/eth0* \
      "$ROOT_MNT"/etc/systemd/network/mlan* \
      "$ROOT_MNT"/etc/systemd/network/wlan*

# sysconf.txt: rcn-ee's first-boot reads this to create the user.
{
    echo "user_name=fpp"
    echo "user_password=falcon"
} >> "$ROOT_MNT/boot/firmware/sysconf.txt"

# udev rule for the ASIX USB-Gigabit adapter (per README.BB64).
mkdir -p "$ROOT_MNT/etc/udev/rules.d"
cat > "$ROOT_MNT/etc/udev/rules.d/99-asix.rules" <<'UDEV_EOF'
ACTION=="add", SUBSYSTEM=="usb", ATTR{idVendor}=="0b95", ATTR{idProduct}=="1790", ATTR{bConfigurationValue}!="1", ATTR{bConfigurationValue}="1"
UDEV_EOF

# Writeable resolv.conf for apt/curl inside chroot.
# rcn-ee images symlink /etc/resolv.conf -> /run/systemd/resolve/stub-resolv.conf
# (or similar). cp through that symlink writes into /run, which we then blow
# away by mounting tmpfs over /run a few lines down. Remove the symlink and
# write a real file so DNS works inside the chroot.
rm -f "$ROOT_MNT/etc/resolv.conf"
cp -L /etc/resolv.conf "$ROOT_MNT/etc/resolv.conf"

# qemu-user-static for chroot execution (no-op when host is also arm64,
# but harmless and keeps the script portable to x86_64 build hosts).
install -m 0755 "$(command -v "$QEMU_BIN")" "$ROOT_MNT/usr/bin/$QEMU_BIN"

# Bind mounts for chroot. /run gets a fresh tmpfs (see build-image-pi.sh
# for full rationale -- avoids host's masked-unit state leaking in).
for d in dev dev/pts proc sys; do
    mkdir -p "$ROOT_MNT/$d"
    mount --bind "/$d" "$ROOT_MNT/$d"
    MOUNTS+=("$ROOT_MNT/$d")
done
mkdir -p "$ROOT_MNT/run"
mount -t tmpfs -o mode=755 tmpfs "$ROOT_MNT/run"
MOUNTS+=("$ROOT_MNT/run")

#############################################################################
# 5. Run FPP_Install.sh inside chroot (after bloat removal)
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

echo "      Running BB64 prep + FPP_Install.sh inside chroot..."
cat > "$ROOT_MNT/tmp/fpp-chroot-install.sh" <<CHROOT_EOF
#!/bin/bash
set -euxo pipefail

export DEBIAN_FRONTEND=noninteractive
export FPP_INSTALL_YES=1
# Tell FPP_Install.sh which platform we are -- /sys/class/leds/beaglebone:*
# autodetection doesn't work in chroot.
export FPPPLATFORM_OVERRIDE="BeagleBone 64"
export FPPIMAGEVER="${OSVERSION}"
export FPPBRANCH="${FPPBRANCH}"

# Locale up front so the apt postinst doesn't burn time regenerating en_GB.
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

# Strip stock bloat per README.BB64. Filter to installed packages so missing
# ones don't abort under set -e.
# cockpit* is the rcn-ee web admin console on :9090 -- pointless for FPP (web
# UI only, no desktop) and only bound to localhost, so purge it.
BB_PURGE_CANDIDATES="bb-node-red-installer bb-code-server ti-zephyr-firmware bb-u-boot-beagleboneai64 bb-u-boot-beagleplay bb-u-boot-beagley-ai bb-u-boot-beagleplay-mainline libstd-rust-dev rustc docker.io containerd nginx firmware-nvidia-graphics firmware-intel-graphics cockpit cockpit-ws cockpit-bridge cockpit-system cockpit-packagekit"
# More base-image dev/admin bloat unused by FPP (the dpkg -s filter below skips
# any not present): the full llvm/clang dev stack (~530MB) -- FPP only needs
# clang-format, which keeps its libllvm19/libclang-cpp19 runtime -- plus the
# leftover docker CLI tooling (docker.io/containerd are purged above).
# (mesa-vulkan-drivers / samba-ad-provision get pulled back in as deps by
# FPP_Install, so they are purged AFTER it runs -- see POST_PURGE below.)
BB_PURGE_CANDIDATES="\$BB_PURGE_CANDIDATES llvm-19 llvm-19-dev llvm-19-runtime llvm-19-linker-tools clang-19 clang-tools-19 libclang-rt-19-dev libclang-common-19-dev docker-cli docker-buildx docker-compose"
# BB64 is arm64, so the 32-bit arm-linux-gnueabihf toolchain is a pure cross
# compiler with nothing depending on it. (Do NOT add this on the 32-bit BBB
# image, where arm-linux-gnueabihf is the *native* compiler.)
BB_PURGE_CANDIDATES="\$BB_PURGE_CANDIDATES gcc-arm-linux-gnueabihf gcc-14-arm-linux-gnueabihf cpp-arm-linux-gnueabihf cpp-14-arm-linux-gnueabihf g++-arm-linux-gnueabihf binutils-arm-linux-gnueabihf"
BB_PURGE_INSTALLED=""
for pkg in \$BB_PURGE_CANDIDATES; do
    if dpkg -s "\$pkg" >/dev/null 2>&1; then
        BB_PURGE_INSTALLED="\$BB_PURGE_INSTALLED \$pkg"
    fi
done
if [ -n "\$BB_PURGE_INSTALLED" ]; then
    apt-get remove -y --autoremove --purge \$BB_PURGE_INSTALLED
fi

# Reclaim space from old DTB packages that ship in the rcn-ee image.
rm -rf /opt/source/dtb-5* /opt/source/dtb-6.1-* /opt/source/dtb-6.6-* /opt/source/dtb-6.12-* /opt/source/spi*
rm -rf /opt/bb-code-server /opt/vsx-examples

cd /root
/root/FPP_Install.sh --img --yes --branch ${FPPBRANCH} ${INSTALLER_EXTRA_ARGS}

# Purge packages that FPP_Install pulls back in as (recommended) deps but that
# a headless FPP has no use for. Done here, after the install, because the
# pre-install BB_PURGE above runs before these get dragged in. dpkg -s filter
# keeps it safe if a name isn't present.
#   mesa-vulkan-drivers -- Vulkan ICD (~135MB); FPP video is GStreamer/GL, no Vulkan
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

# Finalization (mirrors SD/README.BB64 post-install cleanup)
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
