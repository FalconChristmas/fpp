#!/bin/bash
#
# FPP macOS uninstall script.
#
# This reverses the FPP-specific pieces created by FPP_Install_Mac.sh. Run it
# from the SAME "media" directory you ran the installer from -- i.e. the
# directory that contains the cloned "fpp" checkout and the data directories
# (sequences, music, config, ...).
#
# The script is deliberately conservative: it removes the clearly FPP-specific
# bits (the fppd launchd agent) without asking, but prompts before touching
# anything that might contain your data or that other software could share
# (the data directories and the Homebrew packages). The default answer to every
# prompt is "no".

ARCH=$(uname -p)
MEDIADIR=$(pwd)

if [ "${ARCH}" == "arm" ]; then
    BREWLOC="/opt/homebrew"
else
    BREWLOC="/usr/local"
fi

# Make brew available even from a non-login shell (mirrors the installer).
if [ -x "${BREWLOC}/bin/brew" ]; then
    eval "$(${BREWLOC}/bin/brew shellenv)"
fi

PLIST="${HOME}/Library/LaunchAgents/falconchristmas.fppd.plist"
DATA_DIRS="backups cache config effects images logs music playlists plugindata plugins scripts sequences tmp upload videos virtualdisplay_assets"

# Prompt helper: returns success only on an explicit y/Y. Default is no.
ask() {
    echo -n "$1 [N/y] "
    read ANSWER
    [ "x${ANSWER}" == "xY" -o "x${ANSWER}" == "xy" ]
}

# Remember up front whether this really looks like an FPP media directory, so we
# can warn before deleting data if the script is run from the wrong place.
HAD_FPP_CLONE=0
if [ -d "${MEDIADIR}/fpp/.git" ] || [ -f "${MEDIADIR}/fpp/www/media_root.txt" ]; then
    HAD_FPP_CLONE=1
fi

echo "FPP macOS Uninstall"
echo ""
echo "This will undo the FPP-specific changes made by FPP_Install_Mac.sh:"
echo "  * unload and remove the fppd launchd agent"
echo "  * stop the php and httpd brew services that FPP started"
echo "  * (optional) remove the cloned 'fpp' source/build directory"
echo "  * (optional) remove the FPP data directories (sequences, music, config, ...)"
echo "  * (optional) uninstall the Homebrew packages FPP requires"
echo ""
echo "Media directory being used: ${MEDIADIR}"
if [ "${HAD_FPP_CLONE}" != "1" ]; then
    echo ""
    echo "NOTE: this directory does not look like an FPP media directory (no ./fpp"
    echo "      checkout found). Double-check you are running from the right place."
fi
echo ""
echo "It does NOT modify ~/.zprofile or your ccache settings, and it does not"
echo "auto-revert the in-place edits the installer made to httpd.conf / php.ini"
echo "(see the note printed at the end)."
echo ""
if ! ask "Do you wish to proceed?"; then
    echo
    echo "Uninstall cancelled."
    echo
    exit
fi

echo ""
echo "Removing the fppd launchd agent"
launchctl bootout gui/$(id -u)/falconchristmas.fppd 2>/dev/null
# Fall back to the deprecated form in case the agent was loaded the old way.
launchctl unload -w "${PLIST}" 2>/dev/null
if [ -f "${PLIST}" ]; then
    rm -f "${PLIST}"
    echo "  removed ${PLIST}"
else
    echo "  no launchd agent found (already removed?)"
fi

echo ""
echo "FPP started the php and httpd brew services. These are shared services, so"
echo "they are only stopped (not uninstalled) here. Stopping httpd needs sudo."
if command -v brew >/dev/null 2>&1; then
    if ask "Stop the php and httpd brew services now?"; then
        brew services stop php 2>/dev/null
        # httpd was started in the system domain (sudo) so stop it there too.
        sudo brew services stop httpd 2>/dev/null
        echo "  services stopped"
    else
        echo "  leaving services running"
    fi
else
    echo "  brew not found on PATH; skipping service stop"
fi

echo ""
echo "The installer keeps almost all of its web/PHP config in self-contained"
echo "drop-in files (httpd extra/fpp.conf, php conf.d/fpp.ini, php-fpm.d/fpp.conf)"
echo "and made only minimal edits to httpd.conf (backed up as httpd.conf.fpp-orig)."
if ask "Remove FPP's web/PHP configuration and restore the originals?"; then
    # httpd: restore the base config from the pristine snapshot (reverts the
    # Listen/MPM edits and the IncludeOptional line), then drop the include file.
    HTTPD_ORIG="${BREWLOC}/etc/httpd/httpd.conf.fpp-orig"
    if [ -f "${HTTPD_ORIG}" ]; then
        cp -p "${HTTPD_ORIG}" "${BREWLOC}/etc/httpd/httpd.conf"
        rm -f "${HTTPD_ORIG}"
        echo "  restored ${BREWLOC}/etc/httpd/httpd.conf"
    fi
    if [ -f "${BREWLOC}/etc/httpd/extra/fpp.conf" ]; then
        rm -f "${BREWLOC}/etc/httpd/extra/fpp.conf"
        echo "  removed ${BREWLOC}/etc/httpd/extra/fpp.conf"
    fi
    # php: remove our drop-ins and re-enable the stock www pool (all versions).
    for f in "${BREWLOC}"/etc/php/*/conf.d/fpp.ini "${BREWLOC}"/etc/php/*/php-fpm.d/fpp.conf; do
        if [ -f "$f" ]; then
            rm -f "$f"
            echo "  removed ${f}"
        fi
    done
    for f in "${BREWLOC}"/etc/php/*/php-fpm.d/www.conf.fpp-off; do
        if [ -f "$f" ]; then
            mv "$f" "${f%.fpp-off}"
            echo "  restored ${f%.fpp-off}"
        fi
    done
else
    echo "  leaving FPP web/PHP configuration in place"
fi

echo ""
if [ -d "${MEDIADIR}/fpp" ]; then
    if ask "Remove the cloned FPP source/build directory at ${MEDIADIR}/fpp?"; then
        # Step out of the directory first in case we are running from inside it.
        cd "${MEDIADIR}"
        rm -rf "${MEDIADIR}/fpp"
        echo "  removed ${MEDIADIR}/fpp"
    else
        echo "  keeping ${MEDIADIR}/fpp"
    fi
else
    echo "No ${MEDIADIR}/fpp directory found; skipping source removal."
fi

echo ""
echo "The following FPP data directories exist under ${MEDIADIR}:"
FOUND_DATA=0
for d in ${DATA_DIRS}; do
    if [ -d "${MEDIADIR}/${d}" ]; then
        echo "    ${MEDIADIR}/${d}"
        FOUND_DATA=1
    fi
done
if [ "${FOUND_DATA}" == "1" ]; then
    echo ""
    echo "WARNING: these may contain YOUR data -- sequences, music, playlists,"
    echo "effects, configuration, plugins and logs. Deleting them is permanent."
    if [ "${HAD_FPP_CLONE}" != "1" ]; then
        echo "This also does NOT look like an FPP media directory, so be extra careful."
    fi
    if ask "Permanently delete these FPP data directories?"; then
        for d in ${DATA_DIRS}; do
            rm -rf "${MEDIADIR:?}/${d}"
        done
        echo "  data directories removed"
    else
        echo "  keeping data directories"
    fi
else
    echo "  (none found)"
fi

echo ""
echo "FPP installs these Homebrew packages:"
echo "    php git httpd ccache make zstd wget taglib mosquitto jsoncpp drogon"
echo "    graphicsmagick libusb sdl3 ffmpeg"
echo "Many of these (git, make, php, httpd, ffmpeg, wget, ...) are general-purpose"
echo "tools other software on your Mac may rely on. Only remove them if you are"
echo "sure nothing else needs them. (Packages that other formulae depend on will"
echo "be left in place by brew.)"
if command -v brew >/dev/null 2>&1; then
    if ask "Uninstall these Homebrew packages now?"; then
        brew uninstall php git httpd ccache make zstd wget taglib mosquitto jsoncpp drogon graphicsmagick libusb sdl3 ffmpeg
    else
        echo "  leaving Homebrew packages installed"
    fi
else
    echo "  brew not found on PATH; skipping package removal"
fi

echo ""
echo "Uninstall complete. A few things were intentionally left untouched:"
echo "  * If you declined the config restore above, FPP's drop-in config files and"
echo "    the small httpd.conf edits remain in place. Re-run this script to remove"
echo "    them, or delete the *fpp* files and restore httpd.conf.fpp-orig by hand."
echo "  * Any 'eval \"\$(brew shellenv)\"' line added to ~/.zprofile is left as-is."
echo "  * Global ccache settings are left as-is."
echo ""
