#!/bin/sh
apt-get install -y --no-install-recommends xserver-xorg x11-xserver-utils xinit openbox
apt-get install -y --no-install-recommends chromium-browser
apt-get clean

cat > /etc/xdg/openbox/autostart <<EOF

# Disable any form of screen saver / screen blanking / power management
xset s off
xset s noblank
xset -dpms

# Allow quitting the X server with CTRL-ATL-Backspace
setxkbmap -option terminate:ctrl_alt_bksp

# Start Chromium in kiosk mode
sed -i 's/"exited_cleanly":false/"exited_cleanly":true/' ~/.config/chromium/'Local State'
sed -i 's/"exited_cleanly":false/"exited_cleanly":true/; s/"exit_type":"[^"]\+"/"exit_type":"Normal"/' ~/.config/chromium/Default/Preferences
chromium-browser --disable-infobars --kiosk 'http://localhost'

EOF

echo "Enabling AutoLogin"
cat > /etc/systemd/system/getty@tty1.service.d/autologin.conf << EOF
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin fpp --noclear %I $$TERM

EOF

echo "Enabling startup of chromium"
cat >> /home/fpp/.bashrc << EOF

[[ -z \$DISPLAY && \$XDG_VTNR -eq 1 ]] && startx -- -nocursor

EOF
chown fpp:fpp /home/fpp/.bashrc

