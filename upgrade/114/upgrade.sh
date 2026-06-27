#!/bin/bash
#####################################
# Upgrade 114: Restore ~/.profile so login shells source ~/.bashrc.
#
# The fpp user's dotfiles are normally seeded from /etc/skel by adduser, but
# adduser only copies skel when it *creates* the home directory.  On installs
# where /home/fpp already existed when the user was created (e.g. an evicted
# stock UID-1000 user left the directory behind, or the base image shipped it),
# skel is skipped and the user ends up with no ~/.profile.
#
# A bash *login* shell reads /etc/profile and then the first of ~/.bash_profile,
# ~/.bash_login, ~/.profile -- it never reads ~/.bashrc on its own.  With
# ~/.profile missing, ~/.bashrc is never sourced at login, so
# /opt/fpp/scripts/common is not loaded for SSH/console logins, and Kiosk mode's
# startx (which FPP_Kiosk.sh appends to ~/.bashrc) never runs.
#
# This restores ~/.profile (and ~/.bash_logout) from /etc/skel when missing so
# login shells source ~/.bashrc again.  It is a no-op when ~/.profile is already
# present, so it is safe on a healthy system.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 114: Restore ${FPPHOME}/.profile so login shells source .bashrc"

if [ "${FPPPLATFORM}" = "MacOS" ]; then
    echo "  Skipping on MacOS"
    exit 0
fi

RESTORED_PROFILE=0
for skelfile in /etc/skel/.profile /etc/skel/.bash_logout; do
    base=$(basename "${skelfile}")
    if [ ! -f "${skelfile}" ]; then
        continue
    fi
    if [ -e "${FPPHOME}/${base}" ]; then
        echo "  ${FPPHOME}/${base} already present; leaving as-is"
        continue
    fi
    echo "  Restoring ${FPPHOME}/${base} from /etc/skel"
    cp "${skelfile}" "${FPPHOME}/"
    chown ${FPPUSER}:${FPPGROUP} "${FPPHOME}/${base}"
    if [ "${base}" = ".profile" ]; then
        RESTORED_PROFILE=1
    fi
done

if [ "${RESTORED_PROFILE}" -eq 1 ]; then
    # ~/.profile was missing, so login shells were not sourcing ~/.bashrc and
    # Kiosk mode's autologin -> startx never ran.  Flag a reboot so the corrected
    # login flow (and the kiosk) takes effect.
    echo "  Restored ${FPPHOME}/.profile - flagging reboot so the login/kiosk flow re-runs"
    setSetting rebootFlag 1
fi

exit 0
