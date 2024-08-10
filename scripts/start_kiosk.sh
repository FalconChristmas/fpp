#!/usr/bin/bash



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
chromium-browser --disable-infobars --kiosk 