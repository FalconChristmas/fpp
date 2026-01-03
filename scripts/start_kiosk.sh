#!/usr/bin/bash
LOGFILE="/home/fpp/media/logs/start_kiosk.log"
SETTINGS_FILE="/home/fpp/media/settings"
XORG_FILE="/usr/share/X11/xorg.conf.d/40-libinput.conf"
IDENTIFIER='Identifier "libinput touchscreen catchall"'
OPTION_KEY='TransformationMatrix'
OPTION_LINE='        Option "TransformationMatrix" "0 1 0 -1 0 1 0 0 1"'

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/common
TIMEOUT=$(getSetting KioskTimeout)
if [ "x$TIMEOUT" == "x" ]; then
    xset s off
    xset s noblank
    xset -dpms
else
    xset +dpms
    xset dpms "$TIMEOUT" "$TIMEOUT" "$TIMEOUT"
fi
# Allow quitting the X server with CTRL-ATL-Backspace
setxkbmap -option terminate:ctrl_alt_bksp
# Start Chromium in kiosk mode
sed -i 's/"exited_cleanly":false/"exited_cleanly":true/' ~/.config/chromium/'Local State'
sed -i 's/"exited_cleanly":false/"exited_cleanly":true/; s/"exit_type":"[^"]\+"/"exit_type":"Normal"/' ~/.config/chromium/Default/Preferences

# Rotate screen only if rotatescreen = "1"
# Guard: Check to see if rotate screen disabled
if ! grep -qE '^[[:space:]]*KioskRotate[[:space:]]*=[[:space:]]*"1"' "$SETTINGS_FILE"; then
    echo "$(date) Rotate screen disabled – leaving display normal" >> "$LOGFILE"
    chromium-browser --disable-infobars --kiosk
    exit 0
fi

echo "$(date) Rotate screen is enabled" >> "$LOGFILE"

# Guard: checks to see if TransformationMatrix already exists in the file, if not there then add it
if sed -n "/$IDENTIFIER/,/EndSection/ {
        /Option[[:space:]]\+\"$OPTION_KEY\"/p
    }" "$XORG_FILE" | grep -q .; then
    echo "$(date) TransformationMatrix already present – no changes to file made" >> "$LOGFILE"
    xrandr --output DSI-1 --mode 720x1280 --rate 60 --rotate right
    chromium-browser --disable-infobars --kiosk
    exit 0
else

echo "$(date) Begin of touchscreen file edit" >> "$LOGFILE"
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
echo "$(date) Last Launch Chromium Section after ediing touchscreen file" >> "$LOGFILE"
chromium-browser --disable-infobars --kiosk
