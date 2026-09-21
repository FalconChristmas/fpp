#!/bin/bash
#####################################
# Upgrade 144: Make sure a BeagleBone Black boots FPP's PRU-patched kernel
#
# The 10.1 BBB images shipped with /boot/uEnv.txt pointing at the stock
# BeagleBoard kernel (6.18.52-bone54) instead of FPP's PRU-patched 7.1.6-fpp17,
# so no PRU-driven cape could start -- BBB48String reports "Unable to start PRU"
# on every boot. See issue #2966.
#
# Cause, for the record: the image build installs FPP's kernel as its last
# chroot step, but an "apt-get remove --autoremove bbb.io-kernel-tasks" right
# after it was allowed to INSTALL, and resolved a stock-kernel upgrade that
# "apt-get upgrade" had correctly kept back. That kernel's postinst runs
# zz-uenv_txt, which rewrote uname_r -- last writer wins the boot.
# SD/build-image-bbb.sh now pins the stock kernel out and fails the build if
# the image would boot anything else; this handles the units already flashed.
#
# Both kernels are present on an affected device, so this is only a uEnv.txt
# edit -- nothing is downloaded and no package is touched.
#
# Deliberately driven by what is actually installed rather than by a hardcoded
# version: devices still on 7.1.6-fpp15 or -fpp16 must be repointed at THEIR
# kernel, not at one they do not have. The newest vmlinuz-7.1* wins, and only
# if its modules and DTBs are both present -- pointing u-boot at an incomplete
# kernel would leave the device unbootable, which is far worse than dark pixels.
# A device still on an older FPP kernel series (6.18.x-fppNN) has no
# vmlinuz-7.1* at all and is left untouched.
#
# Idempotent: a device already booting the right kernel changes nothing.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 144: Verify the BeagleBone boots FPP's PRU-patched kernel"

# BBB only. "BeagleBone 64" runs the stock bone kernel by design and the Pis
# do not use uEnv.txt at all.
if [ "${FPPPLATFORM}" != "BeagleBone Black" ]; then
    echo "  Not a BeagleBone Black (${FPPPLATFORM}) - skipping"
    exit 0
fi

UENV="${FPPBOOTDIR}/uEnv.txt"
if [ ! -f "${UENV}" ]; then
    echo "  No ${UENV} - skipping"
    exit 0
fi

# Newest installed FPP 7.1 kernel that is complete enough to boot.
FPP_KV=$(
    for VMLINUZ in "${FPPBOOTDIR}"/vmlinuz-7.1*; do
        [ -f "${VMLINUZ}" ] || continue
        KV="${VMLINUZ##*/vmlinuz-}"
        # u-boot needs the DTB tree; the kernel needs its modules.
        [ -d "/lib/modules/${KV}" ] || continue
        [ -d "${FPPBOOTDIR}/dtbs/${KV}" ] || continue
        echo "${KV}"
    done | sort -V | tail -n1
)

if [ -z "${FPP_KV}" ]; then
    echo "  No complete ${FPPBOOTDIR}/vmlinuz-7.1* found - leaving ${UENV} alone"
    exit 0
fi

CUR_KV=$(sed -n 's/^uname_r=//p' "${UENV}" | head -1)
if [ "${CUR_KV}" = "${FPP_KV}" ]; then
    echo "  Already booting ${FPP_KV} - nothing to do"
    exit 0
fi

echo "  ${UENV}: uname_r=${CUR_KV:-<unset>} -> ${FPP_KV}"

# Build the replacement aside and check it before it goes anywhere near
# /boot. Only the uname_r line changes; every other setting FPP_Install.sh
# put in this file (overlays, cmdline, console) is carried through untouched.
NEWENV=$(mktemp) || exit 1
trap 'rm -f "${NEWENV}"' EXIT

if grep -q '^uname_r=' "${UENV}"; then
    sed "s|^uname_r=.*|uname_r=${FPP_KV}|" "${UENV}" > "${NEWENV}"
else
    { cat "${UENV}"; echo "uname_r=${FPP_KV}"; } > "${NEWENV}"
fi

if [ "$(sed -n 's/^uname_r=//p' "${NEWENV}" | head -1)" != "${FPP_KV}" ] \
    || [ ! -s "${NEWENV}" ]; then
    echo "  ERROR: rewritten uEnv.txt failed its check - ${UENV} left as it was"
    exit 1
fi

# cat rather than mv: keeps the original inode, owner and mode on a file
# u-boot has to be able to read.
if ! cat "${NEWENV}" > "${UENV}"; then
    echo "  ERROR: could not write ${UENV}"
    exit 1
fi
sync

echo "  ${UENV} updated - reboot to start running ${FPP_KV}"
setSetting rebootFlag 1

exit 0
