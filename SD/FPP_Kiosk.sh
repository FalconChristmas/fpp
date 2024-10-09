#!/bin/bash

. /opt/fpp/scripts/common
. /opt/fpp/scripts/functions

apt-get update
if  [ "${FPPPLATFORM}" = "Raspberry Pi" ]; then
    # need this to initialize the GPU on the Pi5
    apt-get install -y gldriver-test
fi
apt-get install -y --no-install-recommends xserver-xorg x11-xserver-utils xinit openbox
apt-get install -y --no-install-recommends chromium-browser
apt-get clean

cat > /etc/xdg/openbox/autostart <<EOF
/opt/fpp/scripts/start_kiosk.sh
EOF

echo "Enabling AutoLogin"
cat > /etc/systemd/system/getty@tty1.service.d/autologin.conf << EOF
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin fpp --noclear %I $$TERM

EOF

echo "Enabling startup of chromium"
cat >> /home/fpp/.bashrc << EOF

[[ -z \$DISPLAY && \$XDG_VTNR -eq 1 ]] && startx -- 

EOF
chown fpp:fpp /home/fpp/.bashrc

echo "kiosk" > /etc/fpp/kiosk

mkdir -p /etc/chromium/policies/managed/
cat > /etc/chromium/policies/managed/policy.json <<EOF
{"RestoreOnStartup": 4,"RestoreOnStartupURLs": ["http://localhost/"]}
EOF

setSetting Kiosk 1
setSetting rebootFlag 1
