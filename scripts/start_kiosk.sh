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

# --- Kiosk display + touchscreen rotation (Raspberry Pi Touch Display 2, 7") ---
# Earlier versions wrote a TransformationMatrix into an xorg.conf.d file (first
# the shared 40-libinput.conf in place, later a dedicated override file). Both
# failed the same way: this script runs *inside* the X session Xorg already
# started, so a config file written here is only picked up by a LATER X
# restart, never the one currently launching chromium. On the very first boot
# after enabling Rotate that left the display rotated but touch still raw --
# confirmed on real Touch Display 2 hardware. Applying the transform live via
# xinput instead takes effect immediately in the running session, and reverting
# it is just resetting the same property to identity -- no file to clean up,
# and nothing persists for an install/uninstall to worry about either. A first
# run also removes any TransformationMatrix truly old versions of this script
# left in the shared 40-libinput.conf (issue #2185 follow-up).
LEGACY_XORG_FILE="/usr/share/X11/xorg.conf.d/40-libinput.conf"
LEGACY_IDENTIFIER='Identifier "libinput touchscreen catchall"'
LEGACY_OPTION_KEY='TransformationMatrix'

if [ -f "$LEGACY_XORG_FILE" ] && sed -n "/$LEGACY_IDENTIFIER/,/EndSection/{
        /Option[[:space:]]\+\"$LEGACY_OPTION_KEY\"/p
    }" "$LEGACY_XORG_FILE" | grep -q .; then
    fppdLogLine "Kiosk" "Removing legacy TransformationMatrix from $LEGACY_XORG_FILE"
    ESC_LEGACY_IDENTIFIER=$(printf '%s\n' "$LEGACY_IDENTIFIER" | sed 's/[.[\*^$(){}+?|]/\\&/g')
    sudo sed -i "/$ESC_LEGACY_IDENTIFIER/,/EndSection/{/Option[[:space:]]\+\"$LEGACY_OPTION_KEY\"/d}" "$LEGACY_XORG_FILE"
fi

# Identify the touchscreen's XInput pointer device generically -- by udev's own
# touchscreen classification, not any one panel's device name -- so this isn't
# tied to the Goodix controller on this particular Touch Display 2 unit.
find_touchscreen_xinput_id() {
    local ev id node
    for ev in /dev/input/event*; do
        udevadm info -q property -n "$ev" 2>/dev/null | grep -q '^ID_INPUT_TOUCHSCREEN=1' || continue
        for id in $(xinput list --id-only 2>/dev/null); do
            node=$(xinput list-props "$id" 2>/dev/null | sed -n 's/.*Device Node.*"\(.*\)"/\1/p')
            [ "$node" == "$ev" ] || continue
            xinput list-props "$id" 2>/dev/null | grep -q "Coordinate Transformation Matrix" && { echo "$id"; return 0; }
        done
    done
    return 1
}
TOUCH_ID=$(find_touchscreen_xinput_id)

ROTATE_OUTPUT=$(getSetting KioskRotateOutput)
if [ "x$ROTATE_OUTPUT" == "x" ]; then
    ROTATE_OUTPUT="DSI-1"
fi
ROTATE_MODE="720x1280"

KIOSK_ROTATE=$(getSetting KioskRotate)
if [ "x$KIOSK_ROTATE" != "x1" ]; then
    fppdLogLine "Kiosk" "Rotate screen disabled - reverting $ROTATE_OUTPUT to normal orientation"
    xrandr --output "$ROTATE_OUTPUT" --rotate normal 2>/dev/null
    if [ -n "$TOUCH_ID" ]; then
        xinput set-prop "$TOUCH_ID" "Coordinate Transformation Matrix" 1 0 0 0 1 0 0 0 1 2>/dev/null
    fi
    $CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
    exit 0
fi

# Confirm the chosen output actually reports the rotated panel mode before
# touching anything -- an output that doesn't exist or isn't this panel would
# otherwise still get a touch transform applied for it.
if ! xrandr 2>/dev/null | awk -v out="$ROTATE_OUTPUT" -v mode="$ROTATE_MODE" '
        $0 !~ /^[[:space:]]/ { grab = ($1 == out) ? 1 : 0; next }
        grab && $1 == mode { found = 1 }
        END { exit !found }
    '; then
    fppdLogLine "Kiosk" "WARNING: $ROTATE_OUTPUT does not report mode $ROTATE_MODE - skipping rotation"
    $CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
    exit 0
fi

if [ -z "$TOUCH_ID" ]; then
    fppdLogLine "Kiosk" "WARNING: no touchscreen input device found - rotating display only"
else
    fppdLogLine "Kiosk" "Applying touch rotation to xinput device $TOUCH_ID"
    xinput set-prop "$TOUCH_ID" "Coordinate Transformation Matrix" 0 1 0 -1 0 1 0 0 1
fi

xrandr --output "$ROTATE_OUTPUT" --mode "$ROTATE_MODE" --rate 60 --rotate right
$CHROMIUM_BIN --disable-infobars --kiosk "$KIOSK_URL"
