#!/bin/bash
#####################################
# Upgrade 136: Take yt-dlp off the distro package and keep it fresh
#
# FPP's Web/HTTP video inputs resolve YouTube URLs by shelling out to yt-dlp.
# The packaged yt-dlp is frozen for the life of the Debian release (trixie
# ships 2025.04.30 and will never move), and YouTube reworks its player every
# few months, so on any box installed more than a few months ago it now fails
# outright -- "The page needs to be reloaded" -- and the video input never
# produces a frame.
#
# Install upstream's standalone build into /usr/local/bin, which precedes
# /usr/bin in fppd's PATH so it shadows the package without removing it, and
# add the daily cron entry that keeps it current from here on.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 136: Install upstream yt-dlp and keep it updated"

if [[ ! -f /etc/debian_version ]]; then
    echo "  Not a Debian system - skipping"
    exit 0
fi

if [ -f /opt/fpp/etc/update-ytdlp ]; then
    echo "  Installing the daily yt-dlp refresh into /etc/cron.daily"
    cp /opt/fpp/etc/update-ytdlp /etc/cron.daily/
    chmod 0755 /etc/cron.daily/update-ytdlp
fi

# Best-effort: a box with no internet keeps the packaged yt-dlp and picks the
# upstream one up on a later daily run, so this must never fail the upgrade.
if [ -x /opt/fpp/scripts/update_ytdlp.sh ]; then
    /opt/fpp/scripts/update_ytdlp.sh || true
fi

exit 0
