#!/bin/bash
#####################################
# Upgrade 127: Clear the old plugin-source settings
#
# The "Unknown Plugins" health check used to classify plugins at read time, by
# matching each installed plugin's repoName against the curated plugin list
# (FalconChristmas/fpp-data pluginList.json).  That produced false positives
# two different ways, and both were recorded permanently in sticky settings:
#
#  1. When the plugin list fetch failed -- no internet, GitHub down, rate limited,
#     freshly imaged, caches reset -- there was nothing to match against, so
#     every plugin on the box classified as "unknown".  The check renders that
#     as a hard failure reading "Reimaging is the only reliable way to ensure
#     this system is not contaminated."
#
#  2. A plugin's own repoName is not always the name the plugin list files it
#     under -- 8 of the 58 current entries differ, e.g. "Statistics-Fpp-Plugin"
#     for repoName "fpp-plugin-AdvancedStats".  Those plugins were reported as
#     unknown even with a perfectly good plugin list in hand.
#
# The flags only record that *something* set them, so an existing value cannot
# be told apart from a correct one after the fact.  Clear them.  Classification
# now happens once, at install time, where FPP has ground truth -- see the
# plugin-source block in www/api/controllers/plugin.php -- so anything installed
# from here on is recorded correctly, and a box whose only "unknown" plugin was
# one of these false positives stops being told to reimage.
#
# The trade is that genuine history about a plugin installed BEFORE this
# upgrade is lost.  That history was not reliable enough to keep, and erring
# toward not accusing is the right direction for a check whose failure text
# tells people to reimage.
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/../../scripts/common

echo "FPP - Upgrade 127: Clear the old plugin-source settings"

if [ ! -f "${SETTINGSFILE}" ]; then
    echo "  No settings file - nothing to do"
    exit 0
fi

# The three keys that have actually shipped.  Nothing else can be on a device.
SETTINGS_TO_CLEAR="PluginOfficialEverInstalled PluginCommunityEverInstalled PluginUnknownEverInstalled"

for SETTING in ${SETTINGS_TO_CLEAR}; do
    if grep -qE "^${SETTING}[[:space:]]*=" "${SETTINGSFILE}"; then
        echo "  Removing ${SETTING}"
        sed -i -E "/^${SETTING}[[:space:]]*=/d" "${SETTINGSFILE}"
    fi
done

echo "  Done - plugin source is now recorded at install time"

exit 0
