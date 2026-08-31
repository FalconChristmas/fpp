#!/usr/bin/bash
SETTINGS_FILE="/home/fpp/media/settings"
XORG_FILE="/usr/share/X11/xorg.conf.d/40-libinput.conf"
IDENTIFIER='Identifier "libinput touchscreen catchall"'
OPTION_KEY='TransformationMatrix'
OPTION_LINE='        Option "TransformationMatrix" "0 1 0 -1 0 1 0 0 1"'

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/common
# The page the kiosk shows comes from the KioskUrl setting.  fppinit's
# setupKiosk() also writes it into chromium's managed RestoreOnStartup policy,
# but a URL given on the command line takes precedence over that policy, so a
# hardcoded one here silently overrides whatever the user configured (#2867).
# Read the setting and pass it through instead.
KIOSK_URL=$(getSetting KioskUrl)
if [ "x$KIOSK_URL" == "x" ]; then
    KIOSK_URL="http://localhost/"
fi

TIMEOUT=$(getSetting KioskTimeout)
if [ "x$TIMEOUT" == "x" ]; then
    xset s off
    xset s noblank
    xset -dpms
else
    xset +dpms
    xset dpms "$TIMEOUT" "$TIMEOUT" "$TIMEOUT"
fi
# Pick which display the kiosk renders on.  On the modesetting (KMS) driver the
# server can select a DPI connector as primary -- the FPP cape's vc4-kms-dpi
# overlay exposes one for LED/pixel output, not as a viewable monitor -- which
# leaves the real HDMI/DSI panel with no signal (the monitor drops to standby).
# Choose the first *connected* output that is not a DPI output and make it
# primary, restoring the pre-Trixie behaviour where HDMI/DSI installs just work.
KIOSK_OUTPUT=$(xrandr 2>/dev/null | awk '$2=="connected" && $1 !~ /^DPI/ {print $1; exit}')
# Making the panel primary is not enough on its own: X sizes its screen to the
# bounding box of every *enabled* output, and the DPI connector stays enabled at
# the pixel framebuffer's geometry (1920x997 for the stock overlay).  That leaves
# the X screen far larger than the panel, so chromium's kiosk window fills 1920x997
# while the panel only shows its top-left 1280x720 -- the page is cut off and its
# responsive breakpoints match 1920px rather than the panel width.  Switch the DPI
# outputs off so the screen matches the panel; DPIPixels programs that connector
# itself through KMS and never renders via X, so pixel output is unaffected.
DPI_OFF=""
for o in $(xrandr 2>/dev/null | awk '$2=="connected" && $1 ~ /^DPI/ {print $1}'); do
    DPI_OFF="$DPI_OFF --output $o --off"
done
if [ -n "$KIOSK_OUTPUT" ]; then
    fppdLogLine "Kiosk" "Using $KIOSK_OUTPUT as the kiosk display"
    if [ -n "$DPI_OFF" ]; then
        fppdLogLine "Kiosk" "Disabling DPI output(s) so the X screen matches $KIOSK_OUTPUT:$DPI_OFF"
    fi
    # One invocation so the server never passes through a state with no output on.
    xrandr --output "$KIOSK_OUTPUT" --auto --primary $DPI_OFF
else
    # Only a DPI output is connected -- disabling it would leave X with no display
    # at all, so leave things alone even though the kiosk has nowhere good to draw.
    fppdLogLine "Kiosk" "WARNING: no non-DPI connected output found; leaving X default display"
fi

# Determine which chromium binary is available (Trixie+ uses 'chromium', older uses 'chromium-browser')
if command -v chromium > /dev/null 2>&1; then
    CHROMIUM_BIN="chromium"
elif command -v chromium-browser > /dev/null 2>&1; then
    CHROMIUM_BIN="chromium-browser"
else
    fppdLogLine "Kiosk" "ERROR: Neither chromium nor chromium-browser found"
    exit 1
fi

# Allow quitting the X server with CTRL-ATL-Backspace
setxkbmap -option terminate:ctrl_alt_bksp
# Start Chromium in kiosk mode
sed -i 's/"exited_cleanly":false/"exited_cleanly":true/' ~/.config/chromium/'Local State'
sed -i 's/"exited_cleanly":false/"exited_cleanly":true/; s/"exit_type":"[^"]\+"/"exit_type":"Normal"/' ~/.config/chromium/Default/Preferences

# Rotate screen only if rotatescreen = "1"
# Guard: Check to see if rotate screen disabled
if ! grep -qE '^[[:space:]]*KioskRotate[[:space:]]*=[[:space:]]*"1"' "$SETTINGS_FILE"; then
    fppdLogLine "Kiosk" "Rotate screen disabled – leaving display normal"
    $CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
    exit 0
fi

fppdLogLine "Kiosk" "Rotate screen is enabled"

# Guard: checks to see if TransformationMatrix already exists in the file, if not there then add it
if sed -n "/$IDENTIFIER/,/EndSection/ {
        /Option[[:space:]]\+\"$OPTION_KEY\"/p
    }" "$XORG_FILE" | grep -q .; then
    fppdLogLine "Kiosk" "TransformationMatrix already present – no changes to file made"
    xrandr --output DSI-1 --mode 720x1280 --rate 60 --rotate right
    $CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
    exit 0
else

fppdLogLine "Kiosk" "Begin of touchscreen file edit"
#sudo sed -i '/Identifier "libinput touchscreen catchall"/,/EndSection/{
#    /EndSection/i\        Option "TransformationMatrix" "0 1 0 -1 0 1 0 0 1"
#}' /usr/share/X11/xorg.conf.d/40-libinput.conf

ESC_IDENTIFIER=$(printf '%s\n' "$IDENTIFIER" | sed 's/[.[\*^$(){}+?|]/\\&/g')
sudo sed -i "/$ESC_IDENTIFIER/,/EndSection/{/EndSection/i\\
$OPTION_LINE
}" "$XORG_FILE"
fi

# Rotate display then launch Chromium
xrandr --output DSI-1 --mode 720x1280 --rate 60 --rotate right
fppdLogLine "Kiosk" "Last Launch Chromium Section after ediing touchscreen file"
$CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
