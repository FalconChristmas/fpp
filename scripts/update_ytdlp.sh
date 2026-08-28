#!/bin/bash
#####################################
# Install / refresh yt-dlp from upstream's own releases.
#
# FPP resolves YouTube URLs for video inputs (VideoInputManager) by shelling
# out to yt-dlp. YouTube reworks its player often enough that a yt-dlp more
# than a few months old stops resolving anything at all: it fails with "The
# page needs to be reloaded" or a bot check, FPP gets no stream URL, and the
# video input never starts.
#
# Debian's packaged yt-dlp is frozen for the life of the release -- trixie
# ships 2025.04.30 and will never move -- so it is already stale by the time an
# image is built and only gets worse. It therefore cannot be the copy FPP
# relies on. This installs upstream's standalone build into /usr/local/bin,
# which precedes /usr/bin in the PATH fppd runs with, so it shadows the package
# without removing it: the package stays behind as the fallback for a box that
# has never had internet, and removing this file reverts to it cleanly.
#
# Usage:
#   update_ytdlp.sh              check and update now
#   update_ytdlp.sh --if-stale   only when the last successful check is older
#                                than STALE_DAYS (this is the cron.daily path)
#
# Always exits 0. A box with no internet, or upstream having a bad day, must
# not fail an FPP install or upgrade over this -- the packaged yt-dlp is still
# there, and the next daily run will try again.
#####################################

BINDIR=$(cd "$(dirname "$0")" && pwd)
. "${BINDIR}/common"

# Overridable so the install path can be pointed somewhere harmless in a test.
INSTALL_PATH="${INSTALL_PATH:-/usr/local/bin/yt-dlp}"
STAMP="${STAMP:-${MEDIADIR}/cache/ytdlp-update-check}"
STALE_DAYS=7
RELEASE_BASE="${RELEASE_BASE:-https://github.com/yt-dlp/yt-dlp/releases/latest/download}"
# Timeouts matter here because SD/FPP_Install.sh calls this synchronously: a
# show network that routes to the internet but never answers must not hold an
# install open. --connect-timeout bounds the dead-network case; --speed-limit
# with --speed-time kills a transfer that connects and then stalls or trickles,
# which --max-time alone would let run for its full ceiling. --max-time stays
# as the hard backstop. Worst case is ~20s for a black-holed network and a few
# seconds when there is no route at all.
CURL="curl -fsSL --connect-timeout 10 --max-time 300 --speed-limit 10000 --speed-time 30 --retry 1"

if [ "$1" = "--if-stale" ]; then
    # The periodic path is opt-out: a show network with no internet, or an
    # operator who wants the binary pinned, sets ytdlpAutoUpdate=0. An explicit
    # run from the command line ignores the setting.
    if [ "$(getSetting ytdlpAutoUpdate)" = "0" ]; then
        exit 0
    fi
    if [ -f "${STAMP}" ] && [ -z "$(find "${STAMP}" -mtime "+${STALE_DAYS}" 2>/dev/null)" ]; then
        exit 0
    fi
fi

# Upstream publishes a standalone binary per architecture, but only as a .zip
# for armv7l. Rather than depend on unzip there, fall back to the plain "yt-dlp"
# asset -- the Python zipapp, which runs anywhere python3 does and which FPP
# already has a python3 for.
ARCH=$(uname -m)
case "${ARCH}" in
    aarch64|arm64)  ASSET="yt-dlp_linux_aarch64" ;;
    x86_64|amd64)   ASSET="yt-dlp_linux" ;;
    *)              ASSET="yt-dlp" ;;
esac

echo "FPP - Refreshing yt-dlp (${ARCH} -> ${ASSET})"

TMPDIR_YT=$(mktemp -d /tmp/fpp-ytdlp.XXXXXX) || exit 0
trap 'rm -rf "${TMPDIR_YT}"' EXIT

if ! ${CURL} -o "${TMPDIR_YT}/${ASSET}" "${RELEASE_BASE}/${ASSET}"; then
    echo "  Could not download ${ASSET} - leaving the current yt-dlp in place"
    exit 0
fi

# Verify against upstream's published checksums before making it executable.
# This is a binary fetched over the network and then run as root's cron job and
# by fppd, so an unverified download is not good enough.
if ${CURL} -o "${TMPDIR_YT}/SHA2-256SUMS" "${RELEASE_BASE}/SHA2-256SUMS"; then
    EXPECTED=$(awk -v a="${ASSET}" '$2 == a { print $1 }' "${TMPDIR_YT}/SHA2-256SUMS")
    ACTUAL=$(sha256sum "${TMPDIR_YT}/${ASSET}" | awk '{print $1}')
    if [ -z "${EXPECTED}" ]; then
        echo "  No checksum published for ${ASSET} - refusing to install"
        exit 0
    fi
    if [ "${EXPECTED}" != "${ACTUAL}" ]; then
        echo "  Checksum mismatch for ${ASSET} - refusing to install"
        exit 0
    fi
else
    echo "  Could not download SHA2-256SUMS - refusing to install an unverified binary"
    exit 0
fi

chmod 0755 "${TMPDIR_YT}/${ASSET}"

# Confirm it actually runs on this box before it replaces a working copy --
# a wrong-architecture asset or a truncated download shows up here, not
# halfway through a show.
NEW_VERSION=$("${TMPDIR_YT}/${ASSET}" --version 2>/dev/null | head -1)
if [ -z "${NEW_VERSION}" ]; then
    echo "  Downloaded yt-dlp does not run here - leaving the current one in place"
    exit 0
fi

OLD_VERSION=$("${INSTALL_PATH}" --version 2>/dev/null | head -1)
if [ "${NEW_VERSION}" = "${OLD_VERSION}" ]; then
    echo "  Already at ${NEW_VERSION}"
    mkdir -p "$(dirname "${STAMP}")" && touch "${STAMP}"
    exit 0
fi

# mv within /usr/local/bin is atomic, and a yt-dlp already running (fppd
# resolving a URL right now) keeps its open inode, so this is safe to do at
# any time.
mv -f "${TMPDIR_YT}/${ASSET}" "${INSTALL_PATH}.new" && mv -f "${INSTALL_PATH}.new" "${INSTALL_PATH}"
if [ $? -ne 0 ]; then
    echo "  Could not install to ${INSTALL_PATH}"
    rm -f "${INSTALL_PATH}.new"
    exit 0
fi

if [ -n "${OLD_VERSION}" ]; then
    echo "  Updated yt-dlp ${OLD_VERSION} -> ${NEW_VERSION}"
else
    echo "  Installed yt-dlp ${NEW_VERSION} (shadowing the packaged $(/usr/bin/yt-dlp --version 2>/dev/null | head -1))"
fi

mkdir -p "$(dirname "${STAMP}")" && touch "${STAMP}"
exit 0
