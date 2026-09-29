#!/usr/bin/bash
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

# --- Raspberry Pi Touch Display 2 rotation ---
# The touch transform is applied live with xinput rather than written to an
# xorg.conf.d file: this script runs inside an X server that has already read
# its config, so a file written here would only take effect on the NEXT start
# and the first boot after enabling Rotate would leave touch unrotated.
ROTATE_MODE="720x1280"
CTM="Coordinate Transformation Matrix"
HAVE_XINPUT=0
if command -v xinput > /dev/null 2>&1; then
    HAVE_XINPUT=1
fi

# Older versions put the matrix into the shared 40-libinput.conf, where it
# applies to every touchscreen whether Rotate is on or not.  Remove it once
# xinput can take over -- but not before: a kiosk installed before xinput was
# a dependency relies on that line, and removing it without xinput present
# would silently leave its touch unrotated.
LEGACY_XORG_FILE="/usr/share/X11/xorg.conf.d/40-libinput.conf"
LEGACY_IDENTIFIER='Identifier "libinput touchscreen catchall"'
LEGACY_OPTION_KEY='TransformationMatrix'
LEGACY_REMOVED=0
LEGACY_KEPT=0
if [ -f "$LEGACY_XORG_FILE" ] && sed -n "/$LEGACY_IDENTIFIER/,/EndSection/{
        /Option[[:space:]]\+\"$LEGACY_OPTION_KEY\"/p
    }" "$LEGACY_XORG_FILE" | grep -q .; then
    if [ "$HAVE_XINPUT" == "1" ]; then
        fppdLogLine "Kiosk" "Removing legacy TransformationMatrix from $LEGACY_XORG_FILE"
        ESC_LEGACY_IDENTIFIER=$(printf '%s\n' "$LEGACY_IDENTIFIER" | sed 's/[.[\*^$(){}+?|]/\\&/g')
        sudo sed -i "/$ESC_LEGACY_IDENTIFIER/,/EndSection/{/Option[[:space:]]\+\"$LEGACY_OPTION_KEY\"/d}" "$LEGACY_XORG_FILE"
        LEGACY_REMOVED=1
    else
        fppdLogLine "Kiosk" "WARNING: xinput not installed - leaving legacy TransformationMatrix in $LEGACY_XORG_FILE"
        LEGACY_KEPT=1
    fi
fi

# Every touchscreen XInput device, found by udev's touchscreen classification
# rather than any one panel's device name -- there can be more than one (e.g.
# a USB touch monitor alongside the panel), so print one id per line rather
# than stopping at the first match. Each device's list-props output is
# fetched once and reused for both the node lookup and the CTM check.
#
# Exclude anything udev tags as USB: a USB touch monitor should keep its own
# touch mapping rather than being remapped onto the kiosk's rotated output.
# The Touch Display 2's I2C-attached controller reports no ID_BUS at all on a
# Pi (confirmed on real hardware), so this only ever excludes genuinely
# USB-attached devices -- it doesn't require a specific bus, just rules one
# out.
find_touchscreen_xinput_ids() {
    local ev id node props udev_props
    local ids
    ids=$(xinput list --id-only 2>/dev/null)
    for ev in /dev/input/event*; do
        udev_props=$(udevadm info -q property -n "$ev" 2>/dev/null)
        printf '%s\n' "$udev_props" | grep -q '^ID_INPUT_TOUCHSCREEN=1' || continue
        printf '%s\n' "$udev_props" | grep -q '^ID_BUS=usb' && continue
        for id in $ids; do
            props=$(xinput list-props "$id" 2>/dev/null)
            node=$(printf '%s\n' "$props" | sed -n 's/.*Device Node.*"\(.*\)"/\1/p')
            [ "$node" == "$ev" ] || continue
            # A given event node maps to exactly one xinput device, so once
            # it's found there's nothing left to check for this ev.
            printf '%s\n' "$props" | grep -q "$CTM" && echo "$id"
            break
        done
    done
}

KIOSK_ROTATE=$(getSetting KioskRotate)
if [ "x$KIOSK_ROTATE" != "x1" ]; then
    # Each kiosk start is a fresh X server, so there is normally nothing to
    # undo -- and resetting the matrix unconditionally would clobber one the
    # user configured for some other panel.  The exception is the legacy line
    # removed above: this X server already loaded it at startup.
    if [ "$LEGACY_REMOVED" == "1" ]; then
        for TOUCH_ID in $(find_touchscreen_xinput_ids); do
            fppdLogLine "Kiosk" "Resetting touch rotation left by the legacy TransformationMatrix (device $TOUCH_ID)"
            xinput set-prop "$TOUCH_ID" "$CTM" 1 0 0 0 1 0 0 0 1
        done
    fi
    $CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
    exit 0
fi

# The panel is whichever connected DSI output offers the portrait panel mode;
# requiring the mode also keeps the touch transform off any other display.
ROTATE_OUTPUT=$(xrandr 2>/dev/null | awk -v mode="$ROTATE_MODE" '
        $0 !~ /^[[:space:]]/ { out = ($1 ~ /^DSI/ && $2 == "connected") ? $1 : ""; next }
        out != "" && $1 == mode { print out; exit }
    ')
if [ -z "$ROTATE_OUTPUT" ]; then
    fppdLogLine "Kiosk" "WARNING: no connected DSI output reports mode $ROTATE_MODE - skipping rotation"
    $CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
    exit 0
fi

# Rotate the display first and confirm it actually worked before touching any
# touch device -- applying the touch transform against a display that's still
# unrotated would rotate touch without rotating the picture.
if ! xrandr --output "$ROTATE_OUTPUT" --mode "$ROTATE_MODE" --rate 60 --rotate right; then
    fppdLogLine "Kiosk" "WARNING: xrandr failed to rotate $ROTATE_OUTPUT - leaving touch unrotated"
    $CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
    exit 0
fi

if [ "$LEGACY_KEPT" == "1" ]; then
    fppdLogLine "Kiosk" "Rotated $ROTATE_OUTPUT; touch rotation comes from the legacy TransformationMatrix"
elif [ "$HAVE_XINPUT" != "1" ]; then
    fppdLogLine "Kiosk" "WARNING: xinput not installed - rotated $ROTATE_OUTPUT but not its touchscreen"
else
    TOUCH_IDS=$(find_touchscreen_xinput_ids)
    if [ -z "$TOUCH_IDS" ]; then
        fppdLogLine "Kiosk" "WARNING: no touchscreen input device found - rotated $ROTATE_OUTPUT only"
    else
        # map-to-output derives the transform from the output's actual
        # current rotation rather than a hardcoded matrix -- confirmed on
        # real Touch Display 2 hardware that a fixed "rotate right" matrix
        # (0 1 0 -1 0 1 0 0 1) comes out 90 degrees wrong on this panel's
        # touch controller, while map-to-output gets it right.
        for TOUCH_ID in $TOUCH_IDS; do
            fppdLogLine "Kiosk" "Mapping touch device $TOUCH_ID to $ROTATE_OUTPUT"
            xinput map-to-output "$TOUCH_ID" "$ROTATE_OUTPUT"
        done
    fi
fi

$CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
