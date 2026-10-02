#!/bin/bash
#####################################
# Upgrade 150: keep the BeagleBoard stock kernel off a BeagleBone Black
#
# Upgrade 144 repointed /boot/uEnv.txt at FPP's PRU-patched kernel, but left
# the stock kernel that hijacked it (issue #2966) installed, together with the
# bbb.io-kernel-* metapackage that keeps it current. The next "apt upgrade"
# on the device pulls a newer stock kernel, its postinst rewrites uname_r in
# uEnv.txt, and every PRU-driven cape stops again.
#
# New images close that for good (SD/build-image-bbb.sh): an apt pin that
# refuses any stock bone kernel, and no stock kernel or metapackage installed.
# This brings an existing device to the same state:
#   * writes the same /etc/apt/preferences.d/fpp-kernel-pin
#   * purges the bbb.io-kernel/headers metapackages and every stock
#     linux-image/headers-*-bone* package except the one uEnv.txt boots
#   * re-asserts uname_r afterwards, since a kernel postrm may have rewritten
#     it, and refuses to leave the device pointing at a kernel that is gone
#
# FPP's own kernels (*-fpp*) are never touched, and nothing is downloaded.
# A device that still boots a stock kernel (no complete FPP kernel, which
# upgrade 144 also left alone) gets the pin only.
#
# Idempotent: a device with the pin and no stock kernel changes nothing.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 150: Keep the stock bone kernel off a BeagleBone Black"

# BBB only. "BeagleBone 64" runs the stock bone kernel by design.
if [ "${FPPPLATFORM}" != "BeagleBone Black" ]; then
    echo "  Not a BeagleBone Black (${FPPPLATFORM}) - skipping"
    exit 0
fi

PIN=/etc/apt/preferences.d/fpp-kernel-pin
NEWPIN=$(mktemp) || exit 1
trap 'rm -f "${NEWPIN}"' EXIT
cat > "${NEWPIN}" <<'PIN_EOF'
# Managed by FPP (SD/build-image-bbb.sh). FPP runs its own PRU-patched kernel;
# installing a stock bone kernel rewrites /boot/uEnv.txt and stops every
# PRU-driven cape from working. Do not remove.
Package: linux-image-*-bone* linux-headers-*-bone*
Pin: release *
Pin-Priority: -1

Package: bbb.io-kernel-* bbb.io-headers-*
Pin: release *
Pin-Priority: -1
PIN_EOF

if cmp -s "${NEWPIN}" "${PIN}"; then
    echo "  ${PIN} already in place"
else
    mkdir -p /etc/apt/preferences.d
    if ! cat "${NEWPIN}" > "${PIN}"; then
        echo "  ERROR: could not write ${PIN}"
        exit 1
    fi
    chmod 644 "${PIN}"
    echo "  wrote ${PIN}"
fi

UENV="${FPPBOOTDIR}/uEnv.txt"
BOOT_KV=$(sed -n 's/^uname_r=//p' "${UENV}" 2>/dev/null | head -1)
case "${BOOT_KV}" in
    *-fpp*) ;;
    *)
        echo "  ${UENV} boots '${BOOT_KV:-<unset>}', not an FPP kernel - leaving installed kernels alone"
        exit 0
        ;;
esac
if [ ! -f "${FPPBOOTDIR}/vmlinuz-${BOOT_KV}" ] || [ ! -d "/lib/modules/${BOOT_KV}" ] \
    || [ ! -d "${FPPBOOTDIR}/dtbs/${BOOT_KV}" ]; then
    echo "  ${BOOT_KV} is incomplete - leaving installed kernels alone"
    exit 0
fi

# dpkg-query, not an apt-get pattern: apt-get treats an unmatched name as a
# regex, where "bbb.io-kernel-*" would not mean what it looks like it means.
STOCK_PKGS=$(dpkg-query -W -f='${db:Status-Abbrev} ${Package}\n' \
        'bbb.io-kernel-*' 'bbb.io-headers-*' 'linux-image-*-bone*' 'linux-headers-*-bone*' 2>/dev/null \
    | awk '$1 ~ /^[ih]/ {print $2}' \
    | grep -vxF -e "linux-image-${BOOT_KV}" -e "linux-headers-${BOOT_KV}" | tr '\n' ' ')

if [ -z "${STOCK_PKGS// /}" ]; then
    echo "  No stock kernel packages installed - nothing to remove"
    exit 0
fi

echo "  Removing: ${STOCK_PKGS}"
export DEBIAN_FRONTEND=noninteractive
if ! apt-get -o DPkg::Lock::Timeout=60 remove -y --purge ${STOCK_PKGS}; then
    # The pin alone already stops a stock kernel from coming back, so this is
    # not worth holding later upgrades back for.
    echo "  WARNING: could not remove the stock kernel packages - the apt pin still blocks new ones"
fi

# What dpkg leaves behind: DTBs it never owned, and module dirs that held
# files generated after install. Both lists, since dpkg may have emptied one.
for KV in $( { ls -1 /lib/modules/; ls -1 "${FPPBOOTDIR}/dtbs/"; } 2>/dev/null | sort -u); do
    case "${KV}" in
        "${BOOT_KV}"|*-fpp*) continue ;;
        *-bone*)
            if ! dpkg-query -W -f='${db:Status-Abbrev}' "linux-image-${KV}" 2>/dev/null | grep -q '^[ih]'; then
                echo "  Removing leftover files for ${KV}"
                rm -rf "/lib/modules/${KV}" "${FPPBOOTDIR}/dtbs/${KV}"
            fi
            ;;
    esac
done

# A kernel postrm may have had the last word on uEnv.txt. Only the uname_r
# line is ours to restore.
CUR_KV=$(sed -n 's/^uname_r=//p' "${UENV}" | head -1)
if [ "${CUR_KV}" != "${BOOT_KV}" ]; then
    echo "  ${UENV} was rewritten to '${CUR_KV:-<unset>}' - restoring ${BOOT_KV}"
    if grep -q '^uname_r=' "${UENV}"; then
        sed -i "s|^uname_r=.*|uname_r=${BOOT_KV}|" "${UENV}"
    else
        echo "uname_r=${BOOT_KV}" >> "${UENV}"
    fi
    if [ "$(sed -n 's/^uname_r=//p' "${UENV}" | head -1)" != "${BOOT_KV}" ]; then
        echo "  ERROR: could not restore uname_r=${BOOT_KV} in ${UENV}"
        exit 1
    fi
fi
sync

echo "  Done - booting ${BOOT_KV}"
exit 0
